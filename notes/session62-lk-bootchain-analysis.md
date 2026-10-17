# Session 62 — LK/aboot + BBSS boot-chain analysis (oslo prototype)

Date 2026-10-17. Follows session61 (partition dumps + boot-chain map). Device:
oslo Android prototype (`oslorow`, build AAA787). This note covers **(a) the
`oem switchboot` handler and boot-partition selection**, **(c) the unlock /
verified-boot path**, and sets up **(b) rcause / past-boot analysis**.

Local work: `notes/session61-...md`, dump set
`recon/passport-android-dump/partition-dumps/` (aboot.img = full 4 MB LK).

------------------------------------------------------------------------------
## 0. TL;DR

- **LK (aboot) link base = `0x100BB000`.** Pointers are built with `movw`/`movt`
  (Clang, PC-relative/PIC), not stored literal pools, so xrefs must be resolved
  from the instruction stream. The flat `aboot.img` was produced by concatenating
  modules whose VA↔file deltas differ slightly, so a single global base resolves
  most but not all references (some strings appear referenced against
  neighbouring bases 0x100B B/C/D…000).
- **Boot-partition selection lives in BBSS** (the ROM extension), not LK: it
  detects the active eMMC boot area, defaults to **BOOT0** on failure, loads the
  primary SBL, and can fall back to the **backup SBL** (or an explicit "backup
  boot key combo"). LK then runs from the boot area.
- **`oem switchboot`** (LK) switches the **active boot area boot0↔boot1**
  (the `phyboot0`/`phyboot1` GPTs) and can refuse when the destination has no
  `rimboot`. It is a boot-area switch, not a user-area boot.
- **Verified boot (c):** the HLOS boot image is authenticated with the
  BlackBerry production key unless `is_unlocked`, an insecure build
  (`bbss_insecure`), or a debug token applies. A **blacklist** (RPMB + NV, with
  anti-rollback versions) can veto a boot. Debug tokens are dead (no servers);
  the practical route is the **insecure backup bootchain**.
- **(b) rcause:** a persistent reset-cause table + kernel console ring. Shows a
  prior **fastboot / reboot-to-bootloader** episode and a **reflash from mfi
  AAA742 to sfi AAA787**; no non-stock kernel ever booted. See §4.

------------------------------------------------------------------------------
## 1. LK reverse-engineering setup

`aboot.img` = 4 194 304 B, raw (no ELF header). First bytes:
`05 00 00 00 03 00 00 00 00 00 00 00 | 00 40 0c 10 | e8 10 0f 00 …` then an ARM
vector table (`b` @ `0x28`). The address 0x10160000+ is used throughout.

**Base derivation.** The "Switchboot failed" reference at file `0x4df8` is
`movw r0,#0xb40c` / `movt r0,#0x1016` → VA `0x1016B40C`; the string is at file
`0xb040c` ⇒ **base = 0x100BB000** (VA = base + file_offset). Confirmed
independently by the "failed to switch to the user partition" ref at `0x409c`
(`0x1016B1F8` → file `0xb01f8`).

Tooling: `capstone` (ARM LE) + a `movw`/`movt` resolver
(`Temp/opencode/xrefs.py`, `solve_targets.py`, `findword.py`). Because LK was
linked from several modules, a fraction of string refs resolve against bases
0x10000000 apart in 0x1000 steps (e.g. 0x100BC000, 0x100C0000 …); treat exact
code addresses below as "region", not absolute.

------------------------------------------------------------------------------
## 2. (a) The `oem` command surface (LK `oem` dispatcher)

String table at file ~`0xA0000–0xB6000` (VA ~`0x1016B000`). The `oem` subcommand
help/format strings (in file order):

```
 oem switchboot            oem mmcinfo          oem ddrinfo
 oem enable-charger-screen oem disable-charger-screen
 oem info                  oem bootlog          oem securewipe
 oem grswipe               oem enable-usb-reset oem led:
 oem bootchart             oem getvarp:         oem dmesg
```

Fastboot-side: `fastboot: processing commands`, `authboot command permission
denied`, `fastboot: %s`. The LK fastboot/`oem` gate is **authboot**
(`authboot channel not opened, cannot check permission`).

### 2a. switchboot messages (what the handler does)

```
 failed to get current boot partition
 cannot switchboot from current partition
 Failed setting boot partition %d
 Failed reading boot partition %d
 No GPT in this partition %d
 Failed setting boot partition back to USER
 destination partition does not contain rimboot
 Switching to boot partition %d
 failed to make boot bootable
 Switchboot failed
 failed to switch to the boot partition
 failed to switch to the user partition
```

Reading: `switchboot` (1) reads the **current** boot area/HWI, (2) refuses if it
can't determine it or if switching from the current one is disallowed,
(3) switches the eMMC host to the destination boot area, (4) validates the
destination contains a GPT + `rimboot` (RIM bootloader payload), (5) makes the
boot partition bootable, (6) switches the host back to the **USER** area, and
(7) reports "Switchboot failed" on any error. The helper at file `0x4084`
iterates an array of 0x48-byte name records and selects "user" vs "boot"
suffixes (the partition-access switch); the function at `0x4dd4` validates an
MBR/GPT sector (reads bytes at `[buf+0x1FE]==0x55`, `[buf+0x1FF]==0xAA`) and
prints "Switchboot failed" when the signature is absent.

Supporting strings: `phyboot0` (file 0xb13c8), `phyboot0hwi`, `phyboot1hwi`,
`phyboot1` (file 0xac56c, referenced at code `0x9588`), `bcota` (0xb13c0),
`rimboot`, and the bootchain update block:

```
 update-bootchain / update-new-bootchain   Updating bootchain
 Updating backup bootchain (%d B)          phyboot1
 Backup bootchain update failed: %s        bootchain update succeeded, rebooting...
 bootchain update failed!                  backup_bootchain      bootchain
```

So the LK boot-area model is: **two eMMC boot areas `phyboot0`/`phyboot1`**, each
a GPT holding `hwi, bbss, sbl1r, tzr, rpmr, sdir, abootr` (per session27 §1).
`bcota` stages a `backup_bootchain_*.pkg`; when the BCB says an update is
available, LK flashes it into `phyboot1`.

### 2b. Boot-partition selection at boot = BBSS

`recon/.../backup_bootchain/parts_secure/bbss.img` strings (file 0x3d4–0x560):

```
 BBSS started            Version: %s
 boot partition detection failure; default BOOT0
 Boot partition: %d
 nvuser                  BCB indicates update is available
 Backup boot key combo active
 Failed to load primary SBL, will load backup SBL instead
 Failed to load backup SBL        Jump to SBL       SBL returned!
 power-on                bbss_wp_type      backup_bc_ver
```
and the loader half (file 0x17e4–0x1a04):

```
 PRIMARY   Loading %s SBL        BACKUP
 boot partition detection failure; default BOOT0
 Failed to find the SBL partition      Failed to read the SBL header
 Invalid SBL header                    Authenticating SBL
 Failed to read SBL image              SBL authentication failed!
 Primary SBL version of %u is too old, expecting %u   sbl_anti_rollback_ver
```

So the **first decision point is BBSS**: it reads the active boot area (the
`ext_csd[179] PARTITION_CONFIG` selection surfaced as `Boot partition: %d`),
defaults to **BOOT0** if detection fails, authenticates SBL, enforces an
anti-rollback version, and can load the **backup** SBL when the primary fails or
the user holds the backup key combo. LK/aboot only runs after SBL.

`oem switchboot` is therefore the runtime way to flip which of `phyboot0` /
`phyboot1` BBSS will load on the next power-on.

------------------------------------------------------------------------------
## 3. (c) Unlock / verified-boot path (debug token excluded)

### 3a. The HLOS gate (LK)

```
 use_signed_kernel=%d, is_unlocked=%d, is_tampered=%d.
 Authenticating boot image (%d): start
 Authenticating boot image with signature %d: done return value = %d
 Boot image verification failed (production key)
 ERROR: Cannot read boot image signature / Cannot read boot image
```

Verification is skipped/relaxed when:

- `is_unlocked=1`, or
- insecure build — `HLOS Image verification failed on insecure build`,
  `Unable to update bsi bbss_insecure`, (`bbss_insecure` kernel cmdline param,
  and BBSS `bsi_update bbss_wp_type failed`), or
- a valid **debug token** — but the tokens are gone:
  - `Debug token hlos_unsigned is active. Skipping image verification.`
  - `hlos_signature debug token not valid or doesn't exist`
  - `Boot image verification failed using debug token key`
  - `Key name in debug token does not match with boot signature`
  - token management: `all_debug_tokens`, `Removing token: %s`,
    `Failed writing token to NV`, `token failed validation`

Per the user, debug tokens are **impossible** to obtain now (BlackBerry's token
servers are gone), so this branch is dead.

### 3b. Blacklist / anti-rollback (LK)

```
 Validating os in boot.img against blacklist
 Using blacklist from boot.img
 Failing boot due to HLOS/bootchain blacklist
 RPMB record and NV partition doesn't exist. Skipping hlos blacklist check
 NV doesn't exist. rpmb bl_version (%d), boot.img bl_version (%d)
 NV Signature verification failed, proceeding with boot anyways
 Failing boot due to NV Signature verification failure
```

So even a signature-valid image is refused if its bootchain/HLOS version is on
the RPMB/NV blacklist (defence against downgrade). This is the reason a *modified*
signed boot image won't just work.

### 3c. The viable unlock route — insecure backup bootchain

Already scoped in session27 §5; re-confirmed here:

- The device carries `backup_bootchain_insecure.pkg` (version **AAA624**, with
  **both** the secure and insecure CA sets) alongside
  `backup_bootchain_wp-power_on_secure.pkg` (AAA625). The insecure package yields
  an insecure SBL/aboot (`bbss_insecure=true`) with **no authboot gate**.
- Mechanism: stage the package into `bcota` and set the BCB (the local
  `unlock.sh` does `cat …_secure.pkg > by-name/bcota; setprop sys.powerctl
  reboot,oem-0x81`), then LK's **`update-bootchain`** flashes it into the backup
  boot area; reboot and hold the **backup boot key combo** → BBSS loads the
  insecure backup SBL → LK without authboot → `fastboot flash` the LineageOS
  recovery/ROM.
- Note: `unlock.sh` stages the **secure** pkg; the unlock needs the **insecure**
  one (or a pkg whose bootchain flags disabled verification). This is the key
  selector to change.
- `bbss_wp_type=power-on` (not fused) means the boot areas are writable once WP
  is lifted — the prototype's real freedom (session27 §2).

**Why not the alternatives**
- Debug token — servers gone (dead).
- Raw boot0 write via RAM loader/BootROM — carries the session-28
  full-device-wipe hazard and is the dangerous path.
- `oem switchboot` alone only changes *which* boot area loads; it does not
  disable verification — it is useful only *after* an insecure bootchain is in
  the backup area.

### 3d. Next concrete probe (read-only, safe)

Before any write, confirm on this unit what the **backup boot area** currently
contains and whether an insecure chain is already staged: read `bcota` (done,
blank in session27), `phyboot1` HWI/version props (`backup_bc_ver=AAA625`), and
re-check `bbss_insecure` after a boot to the backup combo. The dump set already
has `bcota`, `oemspare`, `bcota`-adjacent parts.

------------------------------------------------------------------------------
## 4. (b) rcause / past-boot analysis — results

`rcause` = 16 MiB persistent reset-cause + console-log partition (dump:
`recon/passport-android-dump/partition-dumps/rcause`). Only the first ~2 MiB is
non-zero; the log is **linear (not wrapped)** — oldest at low offset, newest at
high offset. Two contiguous structures:

**(i) reset-cause record block (offset 0x0–~0x1a00).** Header `BBRYRTSCRIni$`,
then ~15 fixed-stride (0x1a4) records of `[build][reason][reboot:cmd=…]`:

| reason | reboot arg |
|---|---|
| `system_server` | `0xdead0002 arg=Battery drained` |
| `unknown` | `0xdead0002 arg=Battery drained` |
| `system_server` | `0xdead0002 arg=null` (x2) |
| `system_server` | **`0xdead0003 arg=bootloader`** |
| **`fastboot`** | **`fastboot reboot`** |
| `system_server` | `0xdead0002 arg=null` |
| `kernel` | (none) |
| `unknown` | `hard reset following soft reset` (x3) |
| `err_fatal__krait_wd` | (none) |

Key: the unit has been **rebooted to the bootloader and used fastboot** in its
history (`0xdead0003 bootloader`, `fastboot reboot`) — consistent with prior
flashing attempts, and with our own fastboot work. `err_fatal__krait_wd` = a
Krait watchdog fatal reset.

**(ii) kernel console-log region (0x100000+).** Contains several partial boots
plus one full dmesg. Two distinct builds appear:

- **AAA742 / `imagetype=mfi`** — `Linux version 3.4.0-grsec-gfcf711e … #1 SMP
  PREEMPT Mon Mar 30 19:40:21 EDT 2015`. Full cmdline recovered, incl.
  `androidboot.serialno=1160724779`, `binfo.hwid=0xf5b242c3`,
  `binfo.bbss_insecure=false`, `binfo.bbss_wp_type=power-on`,
  `binfo.primary_bc_ver=AAA742`, `binfo.backup_bc_ver=AAA625`,
  `androidboot.bootdevice=msm_sdcc.1`. Also LK stage timing:
  `KPI: Bootloader start/end/display/load kernel count = …` and
  `BOARD:Oslo hwid:f5b242c3 rev:131076`.
- **AAA787 / `imagetype=sfi`** — `3.4.0-grsec-g0ea3e38 … #1 SMP PREEMPT Apr 5
  2015` (the current build; matches session61).

So this unit was **reflashed mfi AAA742 → sfi AAA787** at some point (same
serial throughout). **No non-stock (LineageOS) kernel ever appears** — every
kernel is a BlackBerry grsec build; bootchain state in every cmdline is
`bbss_insecure=false`, `bbss_wp_type=power-on`.

**Crashes captured:** a slab allocator panic
`Kernel panic - not syncing: Fatal exception`, `PC is at deactivate_slab+0x164`
reached via `slab_cpuup_callback ← notifier_call_chain ← cpu_notify ←
_cpu_down ← cpu_down ← store_online ← sysfs_write_file` (i.e. a CPU-hotplug vs.
slab race — process `mpdecision`, pid 2056); a prefetch-abort panic at ~21335 s;
and long-running boots with repeated userspace paging faults (`addr 0x14`,
`pgd = …`) out to ~387000 s (~4.5 days). The **newest logged boot** (AAA787)
ends at 79 s with `Restarting system with command 'bootloader'.`, and on that
boot many `/system/bin/*` binaries were `cannot find` (`app_process`/zygote,
`installd`, `keystore`, `sdcard`, `sensors.qcom`, `bkup_bc_update`,
`debuggerd64`, …) — a broken/partial system that then explicitly went to
fastboot. Not yet explained; the one-line implication is a prior incomplete
system write, but verity mounted `dm-0` cleanly, so treat as unresolved.

**Takeaway for the unlock work:** the partition contains no sign of a successful
non-stock boot; it does confirm prior fastboot/bootloader activity and the
power-on WP + secure chain state, and it gives LK stage timings (KPI) but **no
LK/BBSS text logs** (those are not routed here).

------------------------------------------------------------------------------
## 5. Artifacts / tools

- `recon/passport-android-dump/partition-dumps/aboot.img` (LK), `sbl1`, `rcause`,
  `bcota`, and the rest of the session61 dump set.
- `recon/passport-android-dump/vendor/backup_bootchain/parts_secure/bbss.img`
  (+ `abootr/sbl1r/tzr/rpmr/sdir/hwi.img`, certs).
- Analysers (scratch): `Temp/opencode/lk_strings.py`, `find_refs24.py`,
  `scan_low24.py`, `solve_targets.py`, `find_movw.py`, `xrefs.py`, `disasm.py`,
  `grepstr.py`. LK base = `0x100BB000`.
