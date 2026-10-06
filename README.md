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
> The BB10 Passport is currently **non-bootable** — recovery is the priority.
> The Android prototype (`oslo`) is a working one-off build with **no autoloader
> in existence**: do **not** wipe, flash, or unlock it. For educational /
> defensive research on devices the author owns. **At your own risk.**

---

## Device Details

### Primary — BB10 Passport (SQW100-1)

| Field | Value |
|---|---|
| Model | BlackBerry Passport **SQW100-1** |
| Codename | `passport` / `windermere` |
| SoC | Qualcomm **MSM8974AA** (Snapdragon 801) |
| OS / software | **BB10 / QNX 10.3.3.3216** (same OS as the Classic), `BLACKBERRY-603C`, WindermereEMEA |
| Current build | **10.3.3.3216** (rooted autoloader) |
| Previous builds | stock 10.3.3 |
| Carrier / unlock | carrier-unlocked; bootloader locked; boot-partition WP permanent |
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

The Passport was **rooted (uid-0)** and bootable, but after a raw `CMD6`
experiment it entered a **permanent red-blink loop** (`11011`, "Flash Erase
Failure") and is now **non-bootable**. Autoloader recovery fails at ~13%
("Signature Trailer"). Two independent root-cause findings:

- The **erase failure is a policy failure**: an armed security wipe tries to
  erase the boot region against a **fused card-level write-protect**
  (`BOOT_WP[173] = 0x04`, permanent), so BootROM can never complete the erase.
- Separately, **`FS_DIRTY_ALL` is RPMB-backed**, so even a perfect chip-off
  restore of `boot0`/`boot1`/`user` may **not** clear it.

The device's real strength remains that **MSM8974AA is the exact `imggen`
target**, so a no-desolder Android conversion is **mapped but not yet proven**.

**A replacement main board is on the way.** Once it arrives, the Passport will
be returned to service on the new board, and the current (bricked) board becomes
a **dedicated test unit** — experimentation on it will follow (recovery
attempts, UART capture, chip-off/chip-replacement trials, and the
`imggen`/RAM-loader Android path), without risking the working device.

### Android prototype (secondary)

The prototype is **alive and now rooted in the `vold` domain** — a
non-persistent root (page-cache only; a reboot restores the device to its exact
factory state). Its full boot chain, kernel, ramdisk and most partitions have
been dumped, and its SELinux policy has been parsed end-to-end. The device is
treated as a read-only historical specimen: **no flash writes, ever.**

---

## Completed

### BB10 Passport

- **Root** (previously) via the getroot/pathtrust ritual.
- **Driver forensics:** the MMC WP gate (`0xF182`), WP handler (`0x108D0`), and
  the per-open-node `ext` model fully decoded; CID is live-read; no ext_csd cache.
- **eMMC dumps:** `boot0`, `boot1`, `os0` recovered.
- **`FS_DIRTY_ALL` forensics:** shown to be RPMB-backed.
- **`imggen` / `passport_stage3`** decoded (prototype bootloader + PBL debug path).

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

- **Real uid-0 root** (previously) via the getroot/pathtrust ritual.
- **eMMC dumps recovered**: `boot0` (SBL1), `boot1` (blank), `os0` (QNX IFS).
- **Full driver write-protect model decoded** (gate `0xF182`, handler `0x108D0`,
  per-open-node `ext`; CID live-read; no ext_csd cache).
- **`FS_DIRTY_ALL` root-caused to RPMB** — a decisive negative for chip-off restore.
- **Install-seal (`F9 40`) contract root-caused** — explains the red-blink vs `cap.exe`.
- **`imggen` boot0/user images generated and validated offline** (not flashed).

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

- **BB10 Passport recovery.** Parked in the `11011` state until direct eMMC
  access. Highest-value next step: **UART capture** of the FS_DIRTY/Nuke sequence.
- **Prototype: kernel LPE from the dumped `vmlinux`** — unrestricted root to
  read the specific-labeled partitions (`hyp`, `pmic`, `devcfg`, `cmnlib`,
  `keymaster`, `misc`, `nvram`, …) and complete a block-level archive.
- **Prototype: complete archive** — `system`/`userdata` at block level via the
  resident vold-domain root before any conversion is ever considered.

## Failed

### BB10 Passport

- **`rimboot_update` software WP bypass** — its own gate aborts; the NOP-bypass
  fails `EROFS`; forcing the branch arms NV bits but `BOOT_WP` stays `0x04`.
- **NV power-cycle ritual** — decisive negative; WP remains permanent.
- **Autoloader recovery** — fails at the Signature Trailer (~13%).
- **`/dev/mem`** — a decoy (uniform `0xdeadbeef`, no persistence).

### Android prototype (`oslo`)

- **Diagnostics as a file-read primitive** — `FileTask` only checks existence.
- **Binder route to vold/rmt_storage/nvramd/subsystem_ramdump** — only
  `unconfineddomain` has `binder call`; `ddt`/`shell` have `transfer` only.
- **QUIP upload redirect** — pinned to the expired Thawte Premium Server CA.
- **Raw `mmcblk0pN` reads of secure partitions** — SELinux labels are per-inode;
  denied to the vold domain.

## Future Plans

1. **Fit the incoming replacement main board** and return the BB10 Passport to service.
2. On the old (bricked) board: locate **UART test points** and capture the live
   FS_DIRTY/Nuke sequence.
3. Compare **chip-off restore vs full eMMC chip replacement** (a blank chip
   re-provisions RPMB).
4. Once a board boots: the no-desolder `imggen`/RAM-loader Android conversion.
5. **Prototype:** finish the block-level archive (kernel LPE) and keep the unit
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
| `notes/` | Passport session notes (BB10 driver forensics, dumps, EDL, FS_DIRTY; `session22`–`session26` Android prototype: identity/signing, autoloader + root path, hardening + Dirty COW, SELinux policy, vold-payload boot dump) |
| `recon/bootloaders-imggen/` | the `imggen` prototype-bootloader kit (MSM8974) |
| `recon/dumps-passport/` | Passport eMMC dumps (`bb0_full.bin`, `os0_8M.bin`, …) |
| `recon/passport-android-dump/` | prototype `/system` dump (large trees gitignored), root configs, `sepolicy-analysis/policy.cil.gz` |
| `recon/passport-autoloader-certs/` | X.509 chains from retail `boot0`, prototype, balika autoloader |
| `recon/passport-boot-dumps/` | prototype boot-chain images (aboot/sbl1/tz/rpm/sdi/pad), `kernel.bin`, ramdisk text, `SHA256SUMS.txt` (large `boot`/`recovery`/`modem`/`persist`/`oem` kept local) |
| `tools/` | `passport_stage3`, `passport-root/`, `session24/` (Dirty COW), `session25/` (policy parsers), `session26/` (ptracecow2, vold daemon payload) |
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
