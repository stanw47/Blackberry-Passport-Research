# oslo Android prototype — raw partition dumps (session61, 2026-10-17)

Raw eMMC partition images dumped from the **BlackBerry Passport Android prototype
("oslo")**, adb serial `1160724779`, build `AAA787` (Android 5.1 LMY47D),
`primary_bc=AAA787 / backup_bc=AAA625`, `hwid=0xf5b242c3`.

These were read **read-only** from `/dev/block/platform/msm_sdcc.1/by-name/*`
through the session-26 **vold-root channel** (Dirty-COW patch of
`/system/bin/fsck_msdos`, triggered by SD reinsertion; payload then runs as root
in the `u:r:vold:s0` SELinux domain). Nothing was written to the device.

See `notes/session61-oslo-partition-dumps-and-bootchain.md` for the full write-up.

## Partition map (by-name → `mmcblk0pN`)

Sizes from `/proc/partitions` (blocks of 1 KiB):

| pN | blocks | MiB | by-name |
|----|--------|-----|---------|
| p1 | 3 | ~0 | misc |
| p3 | 40 | ~0 | pad |
| p5 | 28672 | 28 | calwork_b |
| p6 | 32768 | 32 | (unidentified) |
| p8 | 4096 | 4 | aboot |
| **p9** | 1024 | 1 | **sbl1** |
| p10 | 512 | 0.5 | tz |
| p11 | 512 | 0.5 | rpm |
| p12 | 512 | 0.5 | sdi |
| p19 | 32 | ~0 | ddr |
| p20 | 32 | ~0 | bkup_ddr |
| p22 | 4 | ~0 | ares |
| p27 | 1024 | 1 | metadata |
| p29 | 4096 | 4 | bcota |
| **p30** | 16384 | 16 | **rcause** |
| p31 | 12288 | 12 | spare |
| p32 | 32768 | 32 | crypto |
| **p33** | 32764 | 32 | **boot** |
| p34 | 4 | ~0 | bootsig |
| p35 | 65536 | 64 | **oem** |
| p36 | 65536 | 64 | **oemspare** |
| p37 | 32764 | 32 | persist |
| p38 | 4 | ~0 | recoverysig |
| **p39** | 32764 | 32 | **recovery** |
| p40 | 4 | ~0 | boardidsig / prdidsig / gsign |
| **p41** | 73728 | 72 | **modem** |
| p42 | 2621440 | 2560 | system (raw) |
| p43 | 1048576 | 1024 | cache |
| p44 | 26333167 | 25716 | userdata |

eMMC: **Samsung `mmc1:0001 032GE4`, 29.1 GiB**; `mmcblk0rpmb` 4 MiB (boot
blacklist counter). The Android system/data are dm-mapped (`dm-0`=system,
`dm-1`=userdata).

## Files

- `aboot.img` — LK/aboot (the verified-boot + unlock authority). 4 MiB.
- `sbl1` — secondary boot loader. 1 MiB.
- `tz` / `rpm` / `sdi` — TrustZone / resource-power-mgr / SDI. 512 KiB each.
- `boot.img` — Android kernel + ramdisk. 33 MiB.
- `recovery.img`, `recovery_new.img` — recovery images (the `_new` one is a
  session-27 attempt at a modified recovery).
- `bcota` — boot-chain OTA slot (consumed by LK's `update-bootchain`).
- `backup_bootchain_wp-power_on_secure.pkg` — the secure backup-bootchain
  package + certs (same file already archived under `../vendor/backup_bootchain/`).
- `rcause` — persistent kernel/console **log ring buffer** (16 MiB).
- `oem`, `oemspare`, `persist`, `crypto`, `dmi_b`, `calwork_b`, `modem`,
  `metadata`, `spare`, `mmcblk0p6.img` — remaining dumped partitions.
- `ares`, `ddr`, `bkup_ddr`, `pad`, `gsign`, `boardidsig`, `prdidsig`,
  `bootsig`, `recoverysig` — small signature / header / padding partitions.
- `p_*.bin` — session61 v6 sweep output (≤2 MiB head of each partition, incl.
  read-denied ones attempted).
- `sepolicy_rootfs.bin` — the SELinux binary policy (read via rootfs
  `/sepolicy`; the `/sys/fs/selinux/policy` node was denied).
- `unlock.sh`, `VOLD*_LOG.txt`, `ddt_ui.xml`, `extcsd_j61.txt` — payload/host
  artifacts from the session.
- `SHA256SUMS.txt` — checksums of everything here.

## Not included (SELinux-denied to `u:r:vold:s0`)

`blog`, `fsg`, `fsc`, `nvuser`, `perm`, `ssd`, `boardid`, `prdid`,
`calback_b`, `mct_b`, `nvram`, `modemst1`, `modemst2`, `frp`. These hold the
device keys/tokens/identity (and the RPWB blacklist lives in `mmcblk0rpmb`),
and the `u:r:vold:s0` domain has no read access to their block devices or to
`/sys/kernel/debug` / `mmc_block_device`. Reaching them needs a different
SELinux domain.

`EXT_CSD[179] PARTITION_CONFIG` could likewise not be read: the debugfs node
`/sys/kernel/debug/mmc1/mmc1:0001/ext_csd` returns **EACCES** on `open`, and
`MMC_IOC_CMD` returns **EINVAL/EPERM**. See the session61 note.
