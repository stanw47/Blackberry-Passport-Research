# BlackBerry Passport (SQW100 / oslo) — Research

> Root, boot-chain, driver-forensics, and no-desolder unlock research on the
> BlackBerry **Passport** — the BB10/QNX retail device (SQW100-1) **and** the
> Android prototype (`oslo`, AAA787).
>
> Part of the **[Blackberry-Research](https://github.com/stanw47/Blackberry-Research)**
> collection · [Williamson Security Solutions](https://williamsonsecuritysolutions.com)

---

## Disclaimer

> **Research aid, not a flashing guide.** Editing eMMC boot partitions or
> toggling write-protect can **permanently brick** a device.
>
> There are two BB10 Passport boards: the **current unit** (replacement board,
> `BLACKBERRY-E538`) is bootable and rooted; the **old board** is parked in the
> `11011` red-blink state. An interrupted BootROM handshake arms a **by-design
> full security wipe** — see `notes/session28`. The Android prototype (`oslo`)
> is a working one-off build with **no autoloader in existence**: do **not**
> wipe, flash, or unlock it. For educational / defensive research on devices
> the author owns. **At your own risk.**

---

## Device Details

### Primary — BB10 Passport (SQW100-1)

| Field | Value |
|---|---|
| Model | BlackBerry Passport **SQW100-1** |
| Codename | `passport` / `windermere` |
| SoC | Qualcomm **MSM8974AA** (Snapdragon 801) |
| OS / software | **BB10 / QNX 10.3.x** — old board `BLACKBERRY-603C`; current unit `BLACKBERRY-E538` (`WINDERMEREEMEA_Rev:05`) |
| Current build | **10.3.03.3216** (rooted autoloader) |
| Previous builds | stock 10.3.3 |
| Carrier / unlock | carrier-unlocked; bootloader locked; boot-partition WP **power-on (`B_PWR_WP_EN`), not fused** — live `ext_csd` read (`session29`) |
| SIM | single |

### Secondary — Android prototype (`oslo`)

| Field | Value |
|---|---|
| Device | BlackBerry Passport **Android prototype** (retail-demo unit) |
| Codename | `oslo` / `oslorow`, serial `1160724779` |
| SoC | Qualcomm **MSM8974PRO-AA** (Snapdragon 801) |
| OS | **native Android 5.1** (LMY47D), build **AAA787** (Apr 5 2015), `androidboot.imagetype=sfi` |
| Kernel | **3.4.0-grsec-g0ea3e38** (Apr 5 2015) |
| Display / input | 1440x1440, 3-row Passport keyboard |
| Demo config | PIN watermark `0x2FFE921C`, `com.avengers.retaildemo` |
| Secure state | `bbss_wp_type=power-on`, `bbss_insecure=false`, dm-verity active, hwid `0xf5b242c3` |
| Signing | **production PKI** — identical BB Root CA / Attestation CA to retail `boot0`; no engineering key |
| Recovery | **none exists** — do not wipe |

---

## Current Status

### BB10 Passport (primary)

**Current unit (replacement board, `BLACKBERRY-E538`)** — bootable and **rooted
(uid-0)** via the `pathtrust`/`btool` → `__root` ritual; reflashed repeatedly
from the pre-rooted autoloader (≈3 min per cycle). Two 2026-10-08 results:

- **Live `ext_csd` read** (`tools/session29-extcsd`): `[170] BOOT_CONFIG_PROT =
  0x00` (not fused), `[173] BOOT_WP = 0x04` = **`B_PWR_WP_EN` — power-on /
  temporary, NOT permanent** (the old "permanent" reading was a bit-numbering
  error), `[179] PARTITION_CONFIG = 0x48` (boot source = `boot0`). The bit is
  re-applied by the boot chain on every boot; the stock driver's
  `WRITE_PROTECT` clear returns `EIO` and changes nothing — the clear must come
  from a loader/autoloader context.
- **RAM-loader session reached BootROM** (`0fca:0001`, model `0x87002C0A`,
  exact loader `LDR_77`) but broke on USB re-enumeration right after
  `set_mode(1)`. An interrupted BootROM password exchange **arms a full security
  wipe by design**: the OS was wiped to `bb10-0010`, then recovered with the
  documented ritual (Power+VolUp+VolDn ≈30–40 s → autoloader reflash) and
  re-rooted. Full write-up: `notes/session28`.

**Old board** — red-blink `11011` ("Flash Erase Failure"); parked until direct
eMMC access. `FS_DIRTY_ALL` is RPMB-backed (chip-off restore may not clear it).

The `MSM8974AA` board is the exact `imggen` target, so a no-desolder Android
conversion remains the goal; the **user-area-boot experiment** (`notes/
session29`) is the current software lane.

### Android prototype (secondary)

The prototype is **alive and now rooted in the `vold` domain** — a
non-persistent root (page-cache only; a reboot restores the device to its exact
factory state). Its full boot chain, kernel, ramdisk and most partitions have
been dumped, and its SELinux policy has been parsed end-to-end. The device is
treated as a read-only historical specimen: **no flash writes, ever.**

---

## Completed

### BB10 Passport

- **Root** (previously, and again on the current unit) via the
  getroot/pathtrust ritual.
- **Driver forensics:** the MMC WP gate (`0xF182`), WP handler (`0x108D0`), and
  the per-open-node `ext` model fully decoded; CID is live-read; no ext_csd cache.
- **eMMC dumps:** `boot0`, `boot1`, `os0` recovered.
- **`FS_DIRTY_ALL` forensics:** shown to be RPMB-backed.
- **`imggen` / `passport_stage3`** decoded (prototype bootloader + PBL debug path).
- **Live `ext_csd` read** on the retail (session29): `BOOT_CONFIG_PROT=0x00`,
  `BOOT_WP=0x04` (power-on), `PARTITION_CONFIG=0x48` — via a new **working
  QNX userland toolchain** (`tools/session29-extcsd`).
- **RAM-loader bring-up to BootROM** with the exact `LDR_77` loader (extracted
  from `loaders.dat`; LZMA-alone payload) — reached `get_var`/`set_mode(1)`.
- **Wipe/recovery ritual** documented (session28): 3-button reset → autoloader
  reflash; root restored via `btool` pathtrust patch.
- **Balika package anatomy**: IFS/RCFS/MBR byte-identical to the stock root
  autoloader; only the UFS record carries the Android 40-partition layout.

### Android prototype (`oslo`)

- **Identity + signing settled:** production PKI shared with retail `boot0`
  (`BB Root CA secure 931828f4…`, `BB Attestation CA secure e55af2b1…`, leaf
  `CN=Bryon Hummel`); the balika Android autoloader's aboot leaf (`495b0ed3…`)
  is the prototype's own aboot leaf.
- **Diagnostics service analysed:** debug token **is** present; `append_log`
  dtype 11 gives **command execution as uid 1301 (`ddt`)** in
  `u:r:diagnostics:s0` — but SELinux denies it `/dev/block` and
  `/data/local/tmp`, and `FileTask` is an existence check only (no read).
- **SELinux policy parsed** (v26, 808 types, 86 classes; the dumped copy was
  CR→CRLF corrupted and was re-pulled). Block-device access mapped per domain.
- **Kernel hardening mapped:** `/proc/self/mem` writes EPERM,
  `process_vm_writev` ENOSYS, `ptrace(PTRACE_POKEDATA)` works.
- **Boot image unpacked:** `kernel.bin` (zImage) → `vmlinux` (16.6 MB, exact
  running kernel) and the real ramdisk (`fstab.qcom`, `init*.rc`,
  `file_contexts`).
- **Balika Android autoloader** decoded: BB10 QCFM container carrying the
  Android boot chain + system.

## Achieved

### BB10 Passport

- **Real uid-0 root** (previously, and restored after each reflash) via the
  getroot/pathtrust ritual (`btool` + `pathtrust !/base/bin/__root`).
- **eMMC dumps recovered**: `boot0` (SBL1), `boot1` (blank), `os0` (QNX IFS).
- **Full driver write-protect model decoded** (gate `0xF182`, handler `0x108D0`,
  per-open-node `ext`; CID live-read; no ext_csd cache).
- **`FS_DIRTY_ALL` root-caused to RPMB** — a decisive negative for chip-off restore.
- **Install-seal (`F9 40`) contract root-caused** — explains the red-blink vs `cap.exe`.
- **`imggen` boot0/user images generated and validated offline** (not flashed).
- **Live boot-WP truth**: `B_PWR_WP_EN` (power-on), not fused — the block is the
  per-boot re-application plus the missing CMD6 path, not a permanent fuse.
- **Working cross-built QNX tool** running as root (`extcsd_probe`), including
  the three-part toolchain fix (stub libc SONAME, minimal crt, `-z now`).

### Android prototype (`oslo`)

- **Dirty COW (CVE-2016-5195) via `PTRACE_POKEDATA`** — working arbitrary
  page-cache write; verified against root-owned `/system/etc/hosts` (restored).
- **Root in the `vold` domain** (uid 0, `u:r:vold:s0`, full generic
  `block_device` read/write) by Dirty-COWing `/system/bin/fsck_msdos` and
  letting vold exec it on a vfat mount; payload forks into a resident daemon
  with a `/sdcard`-based command channel. **Non-persistent** (page cache only).
- **Boot-chain + partition dump:** `boot` (`ANDROID!`), `recovery` (identical
  sha256 to boot), `modem`, `aboot`, `sbl1`, `tz`, `rpm`, `persist`, `sdi`,
  `oem`, `pad` — plus the `backup_bootchain` copies.
- **Kernel + ramdisk extracted** from `boot.img`; `vmlinux` with symbols.
- **Full `/system` pulled** (1.4 GB, 4476 files).

## In Progress

- **No-desolder Android conversion (`session29`).** Phase 0 done (live
  `ext_csd`). Open: where the PBL expects SBL1 in the user area (user-area-boot
  vs boot0-only) — the piece that decides whether `boot0` must be written at
  all. Then: stage the balika Android user area through the signed updater
  (`mod_nvram -d` first) and switch `BOOT_PARTITION_ENABLE` from a
  loader/autoloader context.
- **Autoloader-compilation boot-region write.** BootROM work is deferred until
  an autoloader step can carry it (the only officially-driven way to write the
  `boot0` head).
- **Old board recovery.** Parked in the `11011` state until direct eMMC access;
  highest-value next step: **UART capture** of the FS_DIRTY/Nuke sequence.
- **Prototype: kernel LPE from the dumped `vmlinux`** — unrestricted root to
  read the specific-labeled partitions (`hyp`, `pmic`, `devcfg`, `cmnlib`,
  `keymaster`, `misc`, `nvram`, …) and complete a block-level archive.
- **Prototype: complete archive** — `system`/`userdata` at block level via the
  resident vold-domain root before any conversion is ever considered.

## Failed

### BB10 Passport

- **`rimboot_update` software WP bypass** — its own gate aborts; the NOP-bypass
  fails `EROFS`; forcing the branch arms NV bits but `BOOT_WP` stays `0x04`
  (the NV route cannot clear a power-on WP).
- **NV power-cycle ritual** — decisive negative; WP re-applied every boot.
- **OS-driver WP clear** — `WRITE_PROTECT` CLR (`0xC0201A11`) returns `EIO`;
  `ext_csd[173]` unchanged (`session29`).
- **Raw RAM-loader handshake** — an interrupted BootROM password exchange arms
  the by-design security wipe (USB drop after `set_mode(1)`); recovered by
  reflash (`session28`).
- **Autoloader recovery on the old board** — fails at the Signature Trailer
  (~13%); the current board reflashes fine.
- **`/dev/mem`** — a decoy (uniform `0xdeadbeef`, no persistence).

### Android prototype (`oslo`)

- **Diagnostics as a file-read primitive** — `FileTask` only checks existence.
- **Binder route to vold/rmt_storage/nvramd/subsystem_ramdump** — only
  `unconfineddomain` has `binder call`; `ddt`/`shell` have `transfer` only.
- **QUIP upload redirect** — pinned to the expired Thawte Premium Server CA.
- **Raw `mmcblk0pN` reads of secure partitions** — SELinux labels are per-inode;
  denied to the vold domain.

## Future Plans

1. **User-area-boot experiment** (`session29`): pin the PBL's SBL1 read location,
   then stage the balika Android user area and switch the boot source — the
   no-`boot0` Android lane.
2. **Autoloader-compilation step**: carry the boot-region write (and/or the eMMC
   boot-config switch) through the official updater.
3. On the old board: locate **UART test points** and capture the live
   FS_DIRTY/Nuke sequence; compare **chip-off restore vs full eMMC chip
   replacement** (a blank chip re-provisions RPMB).
4. **Prototype:** finish the block-level archive (kernel LPE) and keep the unit
   read-only. If an Android platform is wanted on hardware, use the signed
   balika autoloader on a **different** Passport — never on this specimen.

---

## Community Activity

- **balika011 (Balazs Triszka)** ported **LineageOS 18.1 (Android 11)** to the
  Passport by swapping the eMMC and reflashing `boot0`/`boot1`; **Guizmox**
  helped stabilise it. Prototype Passports have an **unlocked bootloader** (no
  desoldering needed). An eMMC compatibility list is maintained at balika011.hu.
- **Guizmox** documented the Passport **Android prototypes** (builds AAA249 ...
  AAC014, Android 4.4-5.1) that made the port possible.
- **Zinwa P26** - a 2026 DIY kit replacing the mainboard with a Helio G99,
  12 GB RAM, 256 GB and Android 14 (kit only; an original Passport is required).
- **BerryCore (sw7ft)** - Talkbutton dictation and the BerryCore userland run on
  the Passport.
- Hubs: CrackBerry (the "Passport running Lineage OS 18" thread), XDA,
  balika011.hu, and the BlackBerry community Discord.

## Repository layout

| Path | Contents |
|---|---|
| `notes/` | Passport session notes (BB10 driver forensics, dumps, EDL, FS_DIRTY; `session22`–`session26` Android prototype: identity/signing, autoloader + root path, hardening + Dirty COW, SELinux policy, vold-payload boot dump; `session27` prototype unlock + kernel-LPE groundwork; `session28` retail RAM-loader wipe + re-root; `session29` user-area-boot experiment spec) |
| `recon/bootloaders-imggen/` | the `imggen` prototype-bootloader kit (MSM8974) |
| `recon/dumps-passport/` | Passport eMMC dumps (`bb0_full.bin`, `os0_8M.bin`, …) |
| `recon/passport-android-dump/` | prototype `/system` dump (large trees gitignored), root configs, `sepolicy-analysis/policy.cil.gz` |
| `recon/passport-autoloader-certs/` | X.509 chains from retail `boot0`, prototype, balika autoloader |
| `recon/passport-boot-dumps/` | prototype boot-chain images (aboot/sbl1/tz/rpm/sdi/pad), `kernel.bin`, ramdisk text, `SHA256SUMS.txt` (large `boot`/`recovery`/`modem`/`persist`/`oem` kept local) |
| `tools/` | `passport_stage3`, `passport-root/`, `session24/` (Dirty COW), `session25/` (policy parsers), `session26/` (ptracecow2, vold daemon payload), `session29-extcsd/` (working QNX userland toolchain + `extcsd_probe`) |
| `devmaps/` | Passport device map (schema v1.0) |
| `firmware/` | **not committed** — fetch instructions |

---

## Related repos

- **Hub:** [Blackberry-Research](https://github.com/stanw47/Blackberry-Research)
- **Classic** (same BB10 platform): [Blackberry-Classic-Research](https://github.com/stanw47/Blackberry-Classic-Research)
- **Priv** (adjacent MSM8992 lineage): [Blackberry-Priv-Research](https://github.com/stanw47/Blackberry-Priv-Research)

---

## Citations & Acknowledgements

| Source | URL | Relevance |
|---|---|---|
| balika011 — Passport conversion | https://balika011.hu/blackberry/guides/passport/conversion.php | canonical eMMC unlock |
| BBAndroids / imggen | https://github.com/BBAndroids/imggen | prototype bootloader |
| BBAndroids / passport_stage3 | https://github.com/BBAndroids/passport_stage3 | PBL debug-mode path |
| Oleksandr / bb10.root.sx | https://bb10.root.sx | RAM-loader / raw-MMC research |
| CVE-2016-5195 (Dirty COW) | https://nvd.nist.gov/vuln/detail/CVE-2016-5195 | page-cache write primitive used on `oslo` |
| Linux 3.4 ARM syscall tables | https://github.com/torvalds/linux/blob/v3.4/arch/arm/include/asm/unistd.h | syscall-only ARM payloads |

---

## License

Research notes and original scripts are provided for educational purposes;
third-party code retains its own license.
