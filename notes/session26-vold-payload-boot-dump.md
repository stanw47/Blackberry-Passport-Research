# Session 26 — Root in the vold domain via Dirty COW + full boot-chain dump

Date: 2026-10-06. Follow-up to session24/25. Goal: turn the Dirty COW write
primitive + the sepolicy map into a working root primitive and dump the
partitions. **Done.**

---

## 1. The path: vold helper execs run in the vold domain

Session 25 established: `rmt_storage`/`vold`/`nvramd`/`subsystem_ramdump`/
`reset_cause` cannot be binder-called by `shell`/`diagnostics`, and
mount/unmount via `service call mount` is gated by
`MOUNT_UNMOUNT_FILESYSTEMS` (neither uid holds it). But **vold itself** execs
helpers, and per AOSP 5.1 `Volume::mountVol()` **unconditionally** runs
`Fat::check()` before mounting a vfat volume, which execs
`/system/bin/fsck_msdos`. Those helpers inherit the **vold domain** (root +
full `block_device` rw).

So:
1. Dirty-COW `/system/bin/fsck_msdos` (34064 B) with a payload
   (`tools/session26/ptracecow2.c` — binary-payload variant of the ptrace Dirty
   COW; page-cache only).
2. Trigger vold to mount the vfat SD card (physical eject/reinsert; `mount`
   was blocked by permissions).
3. vold runs the payload as **root in `u:r:vold:s0`**.

Verified live: vold logged `Volume sdcard1 state changing 3 (Checking) -> 4
(Mounted)`; the payload's marker log appeared; the mount proceeded (payload
forks, parent exits 0 = "fsck OK").

## 2. Payload = resident daemon + command channel

`tools/session26/vold_daemon.c` (syscall-only ARMv7, static):
- forks; parent exits 0 so vold's fsck wait returns immediately;
- child dumps partitions to `/storage/emulated/legacy/` and then polls
  `/storage/emulated/legacy/vold_cmd.txt` for commands:
  `ls <dir>` and `dump <src> <outname>` (written from adb shell, unlinked after
  read). One card reinsertion = interactive root (vold domain) access.
- Bug fixed mid-way: ARM `O_DIRECTORY` is `0x4000`, not `0x10000` (that is
  `O_DIRECT`), so directory listings returned EINVAL; `dump` (plain
  `O_RDONLY`) always worked.
- By-name path that works: `/dev/block/platform/msm_sdcc.1/by-name/<name>`
  (confirmed by `fstab.qcom`).

## 3. Partitions dumped (root, vold domain)

Named, read successfully:

| partition | size | notes |
|---|---|---|
| boot | 33550336 | `ANDROID!` image |
| recovery | 33550336 | **identical sha256 to boot.img** |
| modem | 75497472 | `EB 3C 90 "MSDOS"` FAT-style modem image |
| aboot | 4194304 | BB container (`05 00 00 00 03 …`) |
| sbl1 | 1048576 | near-identical to the `backup_bootchain` sbl1r |
| tz | 524288 | ELF |
| rpm | 524288 | ELF (exact match to `backup_bootchain` rpmr) |
| persist | 33554432 | |
| sdi | 524288 | |
| oem | 67108864 | |
| pad | 40960 | |

Denied to the vold domain (specific SELinux labels): `hyp`, `pmic`, `devcfg`,
`cmnlib`, `cmnlib32`, `keymaster`, `mdtp`, `mdtpsecapp`, `DDR`, `misc`
(`misc_partition`), `fsg`/`fsc` (`modem_efs_partition_device`), `ssd`, `frst`,
`modemst1/2`, `bootselect`, `frp`, `nvram`, `config`, `factory`, `devinfo`,
`sp1`, `bkup_bc`.

## 4. boot.img unpacked

- `kernel.bin` — 6,648,976 B, ARM zImage (`Linux kernel ARM boot executable
  zImage (<v3.17)`); gzip payload at +18072 decompresses to **`vmlinux` 16.6 MB
  — `Linux version 3.4.0-grsec-g0ea3e38 … Sun Apr 5 2015`** (the exact running
  kernel; full symbols/strings now available).
- `ramdisk.img` — gzip cpio, 1,549,490 B; unpacked to the **real init scripts,
  `fstab.qcom`, `file_contexts`** (these were unreadable on the live device).
- cmdline: `androidboot.hardware=qcom user_debug=31 msm_rtb.filter=0x3b7
  ehci-hcd.park=3 androidboot.bootdevice=msm_sdcc.1
  androidboot.imagetype=sfi build_number=AAA787 trace_event=…`
- `fstab.qcom` confirms: `system` and `userdata` are
  `/dev/block/platform/msm_sdcc.1/by-name/{system,userdata}`, `sdcard1` is
  `voldmanaged`.

## 5. Artifacts

In-repo (small): `recon/passport-boot-dumps/`
- `aboot.img`, `sbl1.img`, `tz.img`, `rpm.img`, `sdi.img`, `pad.img`,
  `kernel.bin`, `SHA256SUMS.txt`, `boot_cmdline.txt`,
  `ramdisk-text/` (fstab, init*.rc, file_contexts, property/seapp/service
  contexts, init*.sh).

Local only (too large to commit; `~/passport-android-dumps/`):
`boot.img`, `recovery.img`, `modem.img`, `persist.img`, `oem.img`,
`vmlinux.bin` (16.6 MB), unpacked `ramdisk/`.

Tools: `tools/session26/ptracecow2.c` (Dirty COW with binary payload file),
`tools/session26/vold_daemon.c` (payload/daemon).

## 6. Device state / safety

- `/system/bin/fsck_msdos` is patched **in page cache only** (a reboot
  restores it).
- A resident root daemon (vold domain) is polling
  `/storage/emulated/legacy/vold_cmd.txt`; a reboot clears both.
- **No flash writes anywhere**; the SD card mounts normally (payload exits 0).
- The prototype still has no autoloader; do not wipe.
