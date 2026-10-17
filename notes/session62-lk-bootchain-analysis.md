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

-----------------------------------------------------------------------------
## 6. Addendum — switchboot handler disassembly + backup-chain state (read-only)

### 6a. Base correction (supersedes §0/§1's single `0x100BB000`)

`aboot.img` is a **multi-segment flatten**: the VA↔file delta differs per module,
so a single global base resolves only part of the refs. Per-region bases verified
by resolving the stack canary (`0x101CC8B8`) and the string table:

| file region | working base | evidence |
|---|---|---|
| `~0x1a700–0x1ac00` (`oem` dispatcher) | `0x100C0000` | §2 strings resolve |
| `~0x1e800–0x1fd00` (charger/NV/switchboot) | `0x100C4000` | canary→off `0x1088B8`; strings→off `0xAxxxx` |
| `~0x22a00–0x22e40` (bootstats/NV helpers) | `0x100C0000` | canary→off `0x10C8B8`; strings→off `0xBxxxx` |

### 6b. switchboot core function (file `0x1F888`, base `0x100C4000`)

Reconstructed control flow (strings → §2a):

1. Save canary, call `0x2C0`/`0x9814` to read the current boot-area record.
2. `ldrb [cur]`; **toggle**: `1→2`, `2→1`; else `failed to get current boot
   partition`.
3. Copy a 16-byte GPT entry from a `.data` table (`0x101724B4`) into a local.
4. `bl 0xA3E8(newpart)` = switch host to that boot area; `!=0` ⇒
   `cannot switchboot from current partition`.
5. `malloc(0x4000)`, read the partition sectors.
6. Validate the destination carries `rimboot` ⇒ else
   `destination partition does not contain rimboot`.
7. `bl 0xA448([cur])` = make boot partition bootable ⇒
   `Switching to boot partition %d` (on failure `failed to make boot bootable`).
8. Restore: `Failed setting boot partition back to USER` /
   `failed to get current boot partition`; propagate errors
   `No GPT in this partition %d`, `Failed reading boot partition %d`,
   `Failed setting boot partition %d`.

Wrapper at `0x1FAC0` calls `0x1F888` and reports `Switchboot failed`. The
best-effort **getter trampolines** at file `0x1EA50` (returns **2**) and `0x1EA8C`
(returns **1**) both tail-branch into the state fn at `0x1E894`.

### 6c. Backup-bootchain writer (file `0x1F61C`, base `0x100C4000`)

Prints `Updating backup bootchain (%d B)`; checks the `Security:` flag and the
HWI region at offset `0x4400` (`Invalid HWI partition`, `GPT header not found`);
selects the boot area, writes the staged image, re-selects (errors
`failed to switch to the boot partition` / `failed to switch to the user
partition`). This is the routine `update-bootchain` drives when the BCB says an
update is available (§2a/§3c).

### 6d. `oem` command registration table (file `0x1FC08`, base `0x100C4000`)

A list of `add_cmd(name, handler)` pairs (handler = VA in the command-
implementation module):

```
oem switchboot          -> 0x100E2A64     oem ddrinfo    -> 0x100E2A28
oem info                -> 0x100E26AC     oem bootlog    -> 0x100E265C
oem disable-charger-scr -> 0x100E2BC0     oem grswipe    -> 0x100E5C3C
is-password-set         -> 0x100E326C     (securewipe /
Switchboot failed       -> 0x100E3A88      enable-usb-reset also present)
```

Note: these handler VAs resolve to file offsets only modulo the (still-unknown)
base of that module, so exact bodies beyond the switchboot writer above were not
chased further.

### 6e. NV / bootstats helper cluster (file `0x22A98–0x22E40`, base `0x100C0000`)

Register get/set primitives (`0x879D0`/`0x87A48`), `Bootstats not cached.
Reading again`, cached boot-partition parsers (`0x22F94`/`0x22FDC`), and an NV
name reader printing `Security:` / `NV %s is %s` / `Could not %s %s` — i.e. the
plumbing behind `oem info` / `oem getvarp:`.

### 6f. **(2) Read-only verification of backup / insecure-bootchain state — DONE**

Live device (no writes), `adb shell getprop`:

```
ro.boot.binfo.primary_bc_ver = AAA787      ro.boot.binfo.backup_bc_ver = AAA625
ro.boot.binfo.bbss_insecure  = false       ro.boot.binfo.bbss_wp_type  = power-on
ro.boot.imagetype = sfi     ro.boot.verity = active   android.software.verified_boot = 1
```

`/system/vendor/backup_bootchain/` contains exactly two world-readable pkgs
(pulled, hashed):

| file | version (hdr) | size | SHA256 |
|---|---|---|---|
| `backup_bootchain_insecure.pkg` | **AAA624** | 3 428 408 | `A5553DBE…B7CE8` |
| `backup_bootchain_wp-power_on_secure.pkg` | **AAA625** | 3 428 408 | `A175F174…BF134` |

Same size; 388 diff runs / ~112 KB differ (the signed image payloads +
signatures). Pkg header = `magic 0x779D7ECE | size 0x00345038 | hdrver 1 |
"AAA6xx"`; body holds a **GPT** (`EFI PART` at `0x238` + backup GPT at `0x82DC`,
sub-GPTs `0x61828`/`0x645D8`/`0x3049C8`), an `ABBC` container and
`bbss`/`rimboot` strings ⇒ a full backup-boot-area image (hwi, bbss, sbl1r, tzr,
rpmr, sdir, abootr), consistent with `parts_secure/`.

**Conclusion:** the installed backup boot area is the **SECURE** chain
(`backup_bc_ver=AAA625`) and `bbss_insecure=false` — **no insecure chain is
staged**. The insecure **AAA624** package *is* present but is **older** than what
is in the backup area; `bkup_bc_update` only auto-upgrades when the package is
**newer**, so staging AAA624 alone may be treated as stale and skipped. This is
the concrete caveat on the §3c unlock route (may need a forced/insecure-marked
update rather than relying on the version-compare path).

`by-name` is still not listable as `shell` (uid 2000, EACCES); the VOLD-domain
root payload / RAM-loader remain the only readers. No write was performed.

New scratch: `Temp/opencode/ann.py` (base+range annotated disasm),
`bc_insecure.pkg` / `bc_secure.pkg`, `pkdcmp.py`, `pkdscan.py`.

### 6g. (2b) Android-side staging + `bcota` content — read-only

**`bkup_bc_update` is the Android-side updater.** From the boot ramdisk
(`Temp/opencode/ramdisk/init.oem.rc`):

```
service bkup_bc_update /system/bin/bkup_bc_update
   seclabel u:r:bkup_bc_update:s0
   class core          user root      group nvram mfg_stp      oneshot
```

It runs on every boot as root in its own SELinux domain
(`bkup_bc_update_exec`, per ramdisk `file_contexts`). The binary itself is
`lstat`-denied to `shell` (can't pull/execute).

**`bcota` is currently staged with the INACTIVE insecure image.** The session61
v6 partition read of `bcota` (`Temp/…/legacy_p_bcota.bin`, 2 MiB = truncated from
the 4 MiB partition) is **byte-identical to the first 2 MiB of
`bc_insecure.pkg` (AAA624)** (100.00% match; vs 96.24% for the secure pkg). It
carries the full pkg header + embedded `bbss` image (`BBSS started`, `Version:
%s`, `bbss_insecure`, `bsi_update bbss_insecure failed`, `HWI data is invalid!`,
`BCB indicates update is available`, `Backup boot key combo active`,
`bootcontrolblock`, `update-bootchain`). Caveat: the **2026-10-07** full `bcota`
dump (`legacy_bcota_full`, 4 MiB) is all-zero, so the image was staged on the
partition **after** that date — most plausibly by one of our own earlier
attempts (the local `legacy_unlock.sh` writes the *secure* pkg, so the staged
image was written by a different/modified path). The generated `bbss` image
inside both pkgs also stamps a payload checksum at file `0x0C` (AAA624
`0x4016EAD8`, AAA625 `0xF6503DD9`).

**State read-out (live, no writes):** `backup_bc_ver=AAA625` (unchanged) ⇒ the
staged AAA624 has **not** been committed to the backup boot area. `bcota` has no
companion `misc`/`bcb` partition (per the 43-name `by-name` list), so the trigger
is the **reboot reason** `oem-0x81` (the value `unlock.sh` passes via
`setprop sys.powerctl reboot,oem-0x81`) plus whatever BCB BBSS/LK consumes.

**Read limits hit (all SELinux, domain `shell`):** `/proc/partitions` empty,
`by-name/*` `ls -l` EACCES (single-node too), `/sys/reset_region/*` EACCES,
`/proc/last_kmsg` & `/sys/fs/pstore` absent, `ro.boot.bootreason` not exported.
Red herring resolved: `/system/bin/bcc` = **Android RenderScript BCC**, not
boot-chain. `/system_signature` (256 B, world-readable) pulled — the
system/dm-verity signature.

⇒ A **live** `bcota`/BCB re-read (to confirm the staged image is still present
*now*) needs the VOLD-domain root payload (microSD dance) or the RAM loader; the
shell domain cannot see it.
