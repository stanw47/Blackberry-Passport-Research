# Session 61 — oslo Android prototype: root-domain partition dumps + boot-chain map

Date 2026-10-17. Device: **BlackBerry Passport Android prototype ("oslo")**, adb
serial `1160724779`, `oslorow` / product `oslo`, build `AAA787`, Android 5.1
LMY47D, MSM8974, kernel `3.4.0-grsec-g0ea3e38`. Follows sessions 26/29 (vold-root
delivery + EXT_CSD probe).

This session was about **reading the eMMC boot configuration and dumping the
partitions around the boot chain**, using the session-26 "vold-root" channel
(Dirty-COW patch of `/system/bin/fsck_msdos`). It also re-confirmed the SELinux
wall around the storage driver and produced a large, complete partition dump set.

Everything here is **read-only against the device** — no partition is written.

------------------------------------------------------------------------------
## 0. TL;DR

- The vold payload runs as **uid 0 with full caps, but in the `u:r:vold:s0`
  domain** — so SELinux (not unix permissions) blocks the interesting nodes.
- The **`EXT_CSD` debugfs node was found and is real**, but its `open()` returns
  **EACCES** from this domain; the `MMC_IOC_CMD` ioctl also fails (EINVAL /
  EPERM). So `EXT_CSD[179]` (the boot-partition selector) is **not readable from
  here** — it needs a different domain (e.g. `fsck`/`shell` with a policy allow)
  or a kernel-side path.
- We still mapped the whole boot chain by reading **LK/aboot** and the
  **backup-bootchain package**, and we **dumped the partition set** (bootloader,
  boot, modem, oem/oemspare, persist, crypto, rcause, bcota, …) to shared
  storage.

------------------------------------------------------------------------------
## 1. How the dump payload runs (delivery recap)

1. Host builds a tiny **static, libc-less ARM payload** (`tools/session61-extcsd-android/`).
2. `ptracecow2` (Dirty COW) patches the **page cache** of `/system/bin/fsck_msdos`
   with the payload (RAM-only; a reboot restores the pristine file — verified by
   pulling and byte-comparing).
3. vold execs `fsck_msdos` as **root in `u:r:vold:s0`** when the microSD is
   re-inserted → the payload runs.
4. The payload **forks** and the parent exits 0 (so vold's fsck wait succeeds and
   the mount proceeds); the child writes results under
   `/storage/emulated/legacy/`, which is visible to plain `adb pull`.

Delivery is triggered by **physically ejecting + re-inserting the SD card**.

------------------------------------------------------------------------------
## 2. Correcting the map: which MMC host, and the node name

Two mistakes from earlier probes were fixed this session (they had made the
first debugfs walk useless and even caused a runaway log):

- The internal eMMC is on **mmc1** (`mmc1:0001`), **not** `mmc0`. `mmc0` had no
  card; `mmc2` is the removable SD.
- The debugfs attribute is spelled **`ext_csd`** (with underscore), not
  `extcsd`.

**The node exists:** `/sys/kernel/debug/mmc1/mmc1:0001/ext_csd`
(`-r-------- root root u:object_r:debugfs:s0`). Reading it is the goal.

Also fixed: a **dirent-parsing bug** in the first payload that looped forever on
empty-name directory entries and grew the log to ~5.7 MB before a reboot stopped
it.

------------------------------------------------------------------------------
## 3. Identity of the payload (from `/proc`)

```
u:r:vold:s0              # /proc/self/attr/current
Uid: 0  0  0  0
CapEff/CapPrm/CapBnd: ffffffffffffffff   # full capabilities
```

So the payload is **fully privileged by capability**, and `SELinux` is the only
thing standing between it and the storage nodes.

------------------------------------------------------------------------------
## 4. SELinux: what the vold-root domain can and cannot touch

| Target | Result | Meaning |
|---|---|---|
| `/sys/kernel/debug/mmc1/mmc1:0001/ext_csd` | `open` → **EACCES (13)** | denied: `debugfs:file read` |
| `/dev/block/mmcblk0` | `open` → **EACCES (13)** | denied: `mmc_block_device:blk_file` |
| `/sys/fs/selinux/enforce` | `open O_RDWR` → **EACCES (13)** | cannot switch to permissive |
| `/sys/fs/selinux/policy` | `open` → **EACCES (13)** | binary policy node not readable |
| `/sepolicy` (on rootfs) | **read OK (215 355 bytes)** | dump succeeded |
| `/file_contexts` | read OK | dumped (rootfs copy) |
| `/proc/cmdline`, `/proc/<pid>/...`, `/sepolicy` | read OK | |
| `by-name/<partition>` nodes | mostly readable (some denied) | the dump channel |
| `dmesg` | read OK | showed the AVC denials |

`dmesg` confirmed the denials as **AVC** events for domain `vold` on class
`debugfs:file` (read) and `mmc_block_device:blk_file` (read/write) — i.e. a
policy limit, **not** a permission/capability error.

**Conclusion:** escalating *within* this execution context is a dead end for the
eMMC boot config; we either (a) get a different SELinux domain, or (b) work from
the partition nodes we *can* read.

------------------------------------------------------------------------------
## 5. The actual target: `EXT_CSD[179] PARTITION_CONFIG`

`EXT_CSD` byte **179** is `PARTITION_CONFIG`; bits **[5:3]** are
`BOOT_PARTITION_ENABLE` (which boot area — boot0/boot1 — the ROM loads):

- `BOOT_PARTITION_ENABLE = 0x1` → boot0 (`phyboot0`)
- `BOOT_PARTITION_ENABLE = 0x2` → boot1 (`phyboot1`)
- `BOOT_CONFIG_PROT = 0x00` → boot config **writable**
- `BOOT_WP = 0x04` → write-protect mode = **power-on**

Both read paths failed:

- **debugfs node** `/sys/kernel/debug/mmc1/mmc1:0001/ext_csd` → `open` **EACCES**.
- **ioctl `MMC_IOC_CMD`** on `by-name/boot` (and `by-name` device nodes):
  - `0xC038B300` (56-byte struct) → **EINVAL (-22)**
  - `0xC048B300` (72-byte struct) → **EPERM (-1)** (despite full caps → likely a
    grsec `capable()` gate or a malformed argument, not a unix-permission issue)

No `extcsd_*.bin` was produced. See `tools/session61-extcsd-android/` for the
payload that tried all of the above.

------------------------------------------------------------------------------
## 6. Boot command line (full, captured live)

```
androidboot.hardware=qcom user_debug=31 msm_rtb.filter=0x3b7 ehci-hcd.park=3
androidboot.bootdevice=msm_sdcc.1 androidboot.imagetype=sfi build_number=AAA787
trace_event=sched:sched_switch,irq:irq_handler_entry
androidboot.emmc=true androidboot.serialno=1160724779 androidboot.baseband=msm
mdss_mdp.panel=1:dsi:0bbry,mdss_dsi_panorama_1440p_cmd
androidboot.binfo.hwid=0xf5b242c3 androidboot.binfo.rev=131076
androidboot.binfo.name=oslorow androidboot.binfo.product=oslo
androidboot.binfo.variant=row androidboot.binfo.bsis_type=bsis_pka
androidboot.binfo.bbss_insecure=false androidboot.binfo.bbss_wp_type=power-on
androidboot.binfo.backup_bc_ver=AAA625 androidboot.binfo.primary_bc_ver=AAA787
loglevel=2
```

(The session boot-dump copy of this was truncated after `irq:irq_handler_entry`;
this is the complete string, including the `binfo.*` security state.)

------------------------------------------------------------------------------
## 7. Partition map

`/proc/partitions` = `mmcblk0` + **p1–p44** + `mmcblk0rpmb`. There is **no**
`mmcblk0boot0/1` node exposed (the eMMC boot areas are not separately
addressable from Linux here).

`by-name` order (43 bootable names + rpmb):

```
userdata cache system modem recoverysig recovery bootsig boot oemspare oem ares
crypto persist spare rcause bcota frp metadata ssd fsg boardidsig boardid
prdidsig prdid bkup_ddr ddr nvuser perm blog modemst2 modemst1 fsc sdi tz rpm
sbl1 aboot calback_b dmi_b calwork_b nvram pad mct_b gsign
```

eMMC part: **Samsung `mmc1:0001 032GE4`**, 29.1 GiB; RPMB 4 MiB (holds the
boot-blacklist counter).

------------------------------------------------------------------------------
## 8. Boot chain and verified boot (from `aboot` + backup-bootchain package)

Chain (from BBSS and LK strings):

```
BootROM → BBSS (BlackBerry Secure Startup, lives in a boot area)
        → SBL  → LK/aboot → kernel (from the "boot" partition) → Android
```

- BBSS picks the boot area: `boot partition detection failure; default BOOT0`,
  `Boot partition: %d`, then `Jump to SBL`; on failure
  `Failed to load primary SBL, will load backup SBL instead`.
- The two boot areas are `phyboot0` / `phyboot1`; which is used is set by
  `EXT_CSD[179]`.
- LK is the **verified-boot + unlock** authority:
  - `use_signed_kernel=%d, is_unlocked=%d, is_tampered=%d`
  - `Authenticating boot image` / `Boot image verification failed (production key)`
  - debug-token path `hlos_unsigned.tkn` (`/nvuser/hlos_unsigned.tkn`,
    `/perm/hlos_signature.tkn`)
  - blacklist: `Failing boot due to HLOS/bootchain blacklist`,
    `Fuse Ct=%d, Blacklist=%d` (kept in RPMB + NV + e-fuses)
  - per-product binding `AUTHBOOT-oslorow`
- User-facing LK commands: `oem switchboot`, `oem mmcinfo`, `oem info`,
  `oem bootlog`, `oem dmesg`, `oem securewipe`, `oem grswipe`,
  `update-bootchain` / `update-new-bootchain`.
- `backup_bootchain_wp-power_on_secure.pkg` is what LK consumes when `bcota`
  signals `BCB indicates update is available` — the session-27 `unlock.sh`
  routes exactly this pkg into `bcota` and reboots with `oem-0x81`.

Android side (from the boot ramdisk, `fstab.qcom`): a standard partition layout —
`/system` = `by-name/system` (ext4, dm-verity `verify=/system_signature`),
`/data` = `by-name/userdata`, plus `cache` / `persist` / `modem`. Kernel+ramdisk
live in the **boot** partition; the system is **not** packed into boot0.

------------------------------------------------------------------------------
## 9. Dump inventory (what we got this session + the prior dump set)

Dumped this session (to `/storage/emulated/legacy/`, pulled to
`recon/passport-android-dump/partition-dumps/`):

- **`aboot`** (4 MiB full), **`boot`** (33 MiB full), **`sbl1`** (1 MiB),
  **`tz` / `rpm` / `sdi`** (512 KiB each) — the bootloader + kernel.
- **`sepolicy`** (215 355 B) — read via the rootfs `/sepolicy` (the
  `/sys/fs/selinux/policy` node was denied).
- **Partition sweep** (v6, ≤2 MiB each): `rcause`, `bcota`, `oemspare`, `spare`,
  `oem`, `pad`, `ares`, `crypto`, `ddr`, `bkup_ddr`, `sdi`, `tz`, `rpm`,
  `boardidsig`, `prdidsig`, `bootsig`, `recoverysig`, `gsign`, `calwork_b`,
  `dmi_b`, `metadata`, `persist`.
- **Denied to vold** (SELinux): `blog`, `fsg`, `fsc`, `nvuser`, `perm`, `ssd`,
  `boardid`, `prdid`, `calback_b`, `mct_b`, `nvram`, `modemst1`, `modemst2`,
  `frp`.
- **Prior dump set (2026-10-07)**, still on device and now archived:
  `aboot`, `boot`, `modem` (75 MiB), `oem`, `oemspare` (67 MiB each), `persist`,
  `crypto`, `recovery`, `dmi_b` (33 MiB each), `calwork_b` (29 MiB), `rcause`
  (16 MiB), `spare` (12 MiB), `bcota` (4 MiB), `metadata`, `sbl1`, `tz`, `rpm`,
  `sdi`, plus identity/header pages (`boardidsig`, `prdidsig`, `bootsig`,
  `recoverysig`, `gsign`, `ares`, `ddr`, `bkup_ddr`, `pad`) and
  `backup_bootchain_wp-power_on_secure.pkg`, `unlock.sh`, `VOLD*_LOG.txt`,
  `ddt_ui.xml`.

`rcause` = the persistent kernel/console **log ring buffer** (it held a past
`Kernel panic - not syncing: Fatal exception`), useful for reading previous
boots.

------------------------------------------------------------------------------
## 10. What's blocked, and why it matters for LineageOS

- `EXT_CSD[179]` **not readable** from the vold domain → can't confirm/choose the
  boot area from userspace here.
- No writable `recovery` partition (recovery is a full-partition image, size 0
  available) → can't drop in a custom recovery.
- LineageOS zip is **AOSP-testkey signed**; LK verified-boot checks a
  **BlackBerry production key** → unsigned HLOS is rejected.
- The debug-token (`hlos_unsigned.tkn`) route is **not viable** — tokens were
  issued by BlackBerry servers that are **gone**.
- Booting an unsigned/alternate OS therefore has to go through the
  **`is_unlocked` / insecure-bootchain** path (the `bcota` +
  `backup_bootchain_*.pkg` + `oem switchboot` machinery, or a boot0 write — the
  latter carries the known full-device-wipe / brick risk).

------------------------------------------------------------------------------
## 11. Artifacts

- Payload + build: `tools/session61-extcsd-android/` (`extcsd_read.c`,
  `build.sh`, `extcsd_read`).
- Dumps (pulled): `recon/passport-android-dump/partition-dumps/` (see
  `SHA256SUMS.txt` there).
- Payload logs: `/storage/emulated/legacy/extcsd_j61.txt` (+ local `j61*.txt`).
- Related prior notes: session24 (hardening/Dirty COW), session25
  (sepolicy block), session26 (vold payload delivery), session27 (prototype
  unlock + kernel LPE), session29 (EXT_CSD probe).
