# Session 67 — oslo device-side probe: is the LK the user-area `aboot` or boot-area `abootr`?

Date 2026-10-18. Follows session66. Device: oslo Android prototype (AAA787),
serial 1160724779, booted to Android; root re-obtained in the **vold domain**
(uid 0, `u:r:vold:s0`) via the Dirty-COW/`fsck_msdos` channel
(`ptracecow2 /system/bin/fsck_msdos /data/local/tmp/root_daemon` + microSD
reinsert → resident `rd_cmd` command server).

Goal: settle §66.3 — whether the running LK is the **user-area `aboot`**
(OS-writable) or the **boot-area `abootr`** (only the BootROM/official-updater
lane can change it).

--------------------------------------------------------------------------------
## 0. TL;DR

- **The eMMC boot area is NOT exposed to Linux.** `/proc/partitions` / `/sys/block`
  show only `mmcblk0p1…p44`, `mmcblk0rpmb`, `mmcblk1` (SD) — **no `mmcblk0boot0/1`**.
  So the boot-area chain cannot be read or written from the OS at all.
- **The user-area `aboot` (`mmcblk0p8`, 2 MiB) and `sbl1` (`mmcblk0p9`, 512 KiB)
  ARE present and OS-writable** (a no-op write-back of `aboot` sector 0 succeeded
  from the vold-root domain). The user-area `aboot` holds the production LK
  (993 400 B of real content; sha256 `faed857c…`).
- **`blog`/`perm` are `vtnvfs_block_device` → denied even to vold**, so the
  BBSS/SBL/LK boot log can't be read from the OS.
- **Fastboot is fully locked:** every `oem *` command → `authboot command
  permission denied`; `getvar all` empty; `version=0.5`. No LK-side info.
- ⇒ The "which area" question is **still open**; it cannot be settled by reading
  boot0 (invisible) or the log (denied). It needs either a functional test or
  offline BBSS handle-table analysis.

--------------------------------------------------------------------------------
## 1. Block map seen from Linux (root, vold domain)

`/proc/partitions` (blocks of 512 B) — **no boot0/boot1**:

`mmcblk0` (30 535 680), `p1`, `p2`…`p44`, `mmcblk0rpmb` (4096), `mmcblk1` (SD, 30 720 000).

`/dev/block/platform/msm_sdcc.1/by-name` (== `bootdevice/by-name`), key entries:

| part | node | name | size |
|------|------|------|------|
| p1 | `gsign` | 3 | 3 KiB |
| p2 | `mct_b` | 2 | 4 KiB |
| p4 | `nvram` | 4 | 4032 KiB |
| p5/p6/p7 | `calwork_b`/`dmi_b`/`calback_b` | 5/6/7 | 28672/32768/65536 KiB |
| **p8** | **`aboot`** | **8** | **4096 blk = 2 MiB** |
| **p9** | **`sbl1`** | **9** | **1024 blk = 512 KiB** |
| p10/p11/p12 | `rpm`/`tz`/`sdi` | 10/11/12 | 512/512/512 KiB |
| p16/p17/p18 | `blog`/`perm`/`nvuser` | 16/17/18 | 256 KiB (vtnvfs) |
| p29 | `bcota` | 29 | 4096 blk = 2 MiB |
| p37/p39 | `boot`/`recovery` | 37/39 | 32764 blk |
| p38/p40 | `bootsig`/`recoverysig` | 38/40 | 4 KiB |

(Device's real `aboot` = 2 MiB and `sbl1` = 512 KiB; our earlier dumps
`partition-dumps/aboot.img` (4 MiB) and `sbl1` (1 MiB) were **zero-padded** — the
real first 2 MiB / 512 KiB match byte-for-byte.)

## 2. Reads / writability

- Read `mmcblk0p8` → 2 097 152 B, sha256 `faed857c4e8d60eb87df48c8024bea97aff954dc266259a48f946ce0718f3458`;
  `rstrip(b'\0')` = **993 400 B**, sha256 `46e1369e…` (matches session30 "proto
  user-area aboot | 993,400 trimmed"). `== aboot.img[:2MiB]` (dump was padded).
- Read `mmcblk0p9` → 524 288 B, `== sbl1[:512K]`.
- **No-op write-back** of `mmcblk0p8` sector 0 (read it, write it back) → **OK**
  (open-for-write allowed). Same for `mmcblk0p29` (`bcota`). ⇒ `aboot` and
  `bcota` are **OS-writable** block_device partitions; `blog`/`perm` are not.
- `dd` to `mmcblk0p16` (`blog`) / `p17` (`perm`) → **Permission denied**
  (label `vtnvfs_block_device`, no vold rule).

## 3. Boot props (`getprop`, LK-provided)

`ro.boot.imagetype=sfi`, `ro.boot.verity=active`, `ro.boot.binfo.bbss_insecure=false`,
`bbss_wp_type=power-on`, `bsis_type=bsis_pka`, `primary_bc_ver=AAA787`,
`backup_bc_ver=AAA625`, `product=oslo`, `name=oslorow`, `variant=row`,
`hwid=0xf5b242c3`, `rev=131076`, `serialno=1160724779`.

## 4. Fastboot (from bootloader)

- `fastboot oem bootlog` / `oem mmcinfo` / `oem info` → **`FAILED (remote:
  'authboot command permission denied')`**.
- `fastboot getvar all` → empty; `getvar version` → `0.5`; `version-bootloader`
  empty. No LK identity leaked.

## 5. Reset-cause partition (`mmcblk0p30`, `rcause`, 16 KiB read)

Records (newest first): build string `AAA787 (master aospl-msm8974 576
qc8974_sfi-user)` + reasons: `user_power_key`, `recovery reboot`,
`reboot:cmd=0xdead0003,arg=recovery`, `arg=bootloader`, `fastboot`,
`hard reset following soft reset`, `user_power_key_failsafe`. → shows past
reboots to recovery/bootloader only; **no record of the boot-chain source.**

## 6. Backup-pkg `abootr` ≠ user-area LK

`parts_secure/abootr.img` (974 848 B, `70836a39…`) and
`parts_insecure/abootr.img` (`b5039e53…`) are both **different** from the
user-area `aboot` LK (993 400 B, `46e1369e…`). So the backup chain carries a
*distinct* LK; this doesn't tell us which one the primary chain uses.

--------------------------------------------------------------------------------
## 7. What would settle it (next)

1. **Offline:** identify BBSS's 3 MMC handles + the device-info flag at `+0x24`
   used by `0x0f8050f1c`, i.e. does the "boot partition" resolve to a boot-area
   handle or the user area? (Extend the session64 BBSS disassembly to `mmcInit`.)
2. **Functional (reversible):** back up the 2 MiB user-area `aboot`, then write a
   *detectable* variant, reboot, and observe. Risk: if it **is** the loaded LK and
   the SBL verifies it, a bad edit falls back to the backup SBL/LK (still boots)
   — but this needs a solid recovery plan (`boot_linux` from the backup chain, or
   the BootROM lane).
3. **`oem switchboot` semantics:** the LK can switch the active chain between the
   *user* partition and the *boot* partition (`failed to switch to the user
   partition` / `Failed setting boot partition back to USER`) — the exact
   default/active source is the missing fact.

--------------------------------------------------------------------------------
## 8. RESULT (offline BBSS) — the primary chain is in the boot area (boot0)

`mmcInit` (`0xf8050da0`) opens **three MMC handles** into a global array
(`get_handle` = `0xf805d564`):

```
handles[i] = get_handle(device = 0, partition = i)      for i = 0, 1, 2
```

The boot-partition detector (`0xf8050f1c`) walks `handles[0..2]`, `mmcGetDeviceInfo`
each, and returns the first whose device-info byte at **`+0x24`** is set;
otherwise it **defaults to index 1** and logs
`boot partition detection failure; default BOOT0`. **So index 1 == BOOT0**, i.e.
the handles are ordered `0 = user area, 1 = boot0, 2 = boot1` (eMMC partition
order). `bbss_load_sbl` then reads the SBL from that partition's GPT
(`0xf8050f60(boot_idx, sbl_guid, 0)`), and the primary/backup SBL come from the
**active boot-area partition**.

⇒ **The primary chain (PBL → BBSS → SBL → LK/`abootr`) is read from the eMMC boot
area (boot0).** The user-area `aboot`/`sbl1` (2 MiB / 512 KiB, OS-**writable**)
are the **alternate** copy that the LK's `switchboot` can select
(`failed to switch to the user partition` / `Failed setting boot partition back to
USER`).

**Consequences for the oslo → LOS route:**
- A user-area `aboot` write **alone** does not change the default chain (the boot
  area is what boots).
- To make the user area boot the chain, the boot source must be switched
  (LK `switchboot` → **denied in fastboot**), or the boot area written
  (BootROM / official-updater lane; `0x2019` NV gate).
- Even then, the SBL authenticates the LK (§64), so a custom LK still needs the
  insecure chain/anchor. The single OS-writable lever (user-area `aboot`/`sbl1`)
  is not the default load path.
