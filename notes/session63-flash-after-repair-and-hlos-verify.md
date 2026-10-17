# Session 63 — FLASH-AFTER-REPAIR bundle, LK flash whitelist, HLOS image verification (oslo prototype)

Date 2026-10-18. Follows session62 (LK/BBSS boot-chain analysis). Device: oslo
Android prototype (`oslorow`, build AAA787, serial 1160724779).
Goal of this session (user): "try the LineageOS recovery" from the
`F:\FLASH-AFTER-REPAIR` bundle. Outcome: recovered the vendor **post-repair
procedure**, mapped LK's fastboot **flash whitelist**, and **proved LK verifies
the HLOS boot/recovery image** (so no custom recovery/boot will run while locked).

Local work: notes/session62-...md; dumps
`recon/passport-android-dump/partition-dumps/`; bundle `F:\FLASH-AFTER-REPAIR\`;
host tool `Temp\opencode\{loschk,bh,sigchk,abootstr,abref,abreg,mv,bm}.py`.

--------------------------------------------------------------------------------
## 0. TL;DR

- **`F:\FLASH-AFTER-REPAIR` = the POST-chip-repair procedure** (README in
  Vietnamese: "used *after* the chip is injected/replaced and written"). It
  assumes the phone is **already repaired/unlocked**: `fastboot flash recovery
  recovery.img` → **VOL-UP+POWER until LED YELLOW** → Format data → Apply from
  ADB → `adb sideload <LOS zip>`. Credited to "balika".
- **LED semantics (from LK strings):** white/blue/red = **fastboot** (normal);
  **yellow = recovery**. LK prints `VOL_UP pressed. Recovery image.` and
  `VOL_DN pressed. Fastboot mode.` and `ERROR: Could not do normal boot.
  Reverting to fastboot mode.`
- **LK fastboot flash is WHITELISTED.** `fastboot flash recovery` and
  `fastboot flash boot` → `OKAY`. Any other name (e.g. `zzz`, and bootloader
  partitions) → `FAILED (remote: 'authboot flash permission denied')`.
  `fastboot oem info` → same denial. `fastboot boot <img>` → `Not supported`.
  `getvar`: `product=MSM8974`, `max-download-size=0x20000000`,
  `partition-size:system=0xa0000000`, `userdata=0x6473fbe00`, `cache=0x40000000`;
  boot/recovery reported size 0; `getvar all` empty.
- **LK VERIFIES the HLOS image.** Proof: flashed the *stock* boot with **only the
  boot-header cmdline changed** (added `androidboot.occamsux=1
  androidboot.selinux=permissive`); LK **rejected it and reverted to fastboot**.
  Re-flashing the pristine stock boot → device boots Android again. ⇒ custom
  recovery (LineageOS) / modified boot will **not** boot while locked.
- **`bootsig` / `recoverysig` partitions are 4096 B and ALL ZERO.** The real gate
  is a token in **`perm`**: LK strings `/perm/hlos_signature.tkn`,
  `/nvuser/hlos_unsigned.tkn`, `Debug token hlos_unsigned is active. Skipping
  image verification.`, `hlos_signature debug token not valid or doesn't exist`.
- Stock **boot.img == recovery.img** (identical, sha256 `64c43e8b…`); modified
  recovery `recovery_new.img` = `4624c22e…`; vendor LOS recovery `8476149b…`
  (15 226 880 B, valid `ANDROID!`).
- **BCB lives in `perm`, not `misc`** (`by-name/misc` open = `0xfffffffe`
  ENOENT). `bkup_bc_update` reads/writes `/nvram/perm/bootcontrolblock`; LK
  **mounts `perm`** and reads the same.
- Everything still walls off: **HLOS verify (LK) + authboot (flash) + dm-verity
  (`/system`) + SELinux (`perm`/BCB)**.

--------------------------------------------------------------------------------
## 1. The `F:\FLASH-AFTER-REPAIR` bundle (vendor post-repair kit)

```
F:\FLASH-AFTER-REPAIR\
  README.md                                   (Vietnamese; post-repair notes)
  1-FLASH-RECOVERY.bat                         fastboot flash recovery recovery.img
  2-SIDELOAD-ROM.bat                           adb sideload lineage-...-wseries.zip
  recovery.img                                 15 226 880 B, ANDROID!, sha256 8476149b…
  lineage-18.1-20250101-UNOFFICIAL-wseries.zip 551 464 874 B (LineageOS 18.1 / A11, GApps)
  platform-tools\{adb.exe,fastboot.exe,...}
  lineage-18.1-...-wseries\
    boot.img 7 471 104 B ANDROID! ; system.new.dat.br ; system.transfer.list ;
    META-INF\com\google\android\{update-binary,updater-script} ; otacert
```

`1-FLASH-RECOVERY.bat` says verbatim: *"The phone should already be in fastboot
(screen may be black, or the LED blinking white/blue/red/green — that is
NORMAL)."* then `fastboot flash recovery recovery.img`, then instructs
**VOLUME UP + POWER until LED YELLOW**, Factory reset → Format data, Back,
Apply update → Apply from ADB, then run `2-SIDELOAD-ROM.bat`.

README notes: the recovery "must match balika's current build (SHA256 8476149b…)";
do **not** use `recovery-adb.img` (sha256 `ce3319d6…`, different); any
`new_user.img` already contains another phone's IMEI. After install, **verify the
IMEI** matches the back of the phone.

> Takeaway: this kit does **not** unlock anything. It is applied to a device that
> has already been unlocked by the chip repair. On our still-locked prototype the
> `fastboot flash recovery` step is accepted but the recovery **will not boot**.

--------------------------------------------------------------------------------
## 2. LK mode / LED strings (aboot.img string table)

```
VOL_UP pressed. Recovery image.                       VA ~0x1016FE30 (module rel.)
VOL_DN pressed. Fastboot mode.                        VA ~0x1016FE54
error in emmc_recovery_init
ERROR: Could not do normal boot. Reverting to fastboot mode.
yellow
AUTHBOOT
```

Partition-name tables found in-string:
- early: `sbl1 sbl2 sbl3 rpm aboot modem_st1 modem_st2 efs2 recovery misc ssd`
- late:  `oem oemspare fsc modemst1 modemst2 fsg gsign mct_b pad nvram calwork_b
          dmi_b calback_b ddr bkup_ddr prdidsig boardidsig rcause ares crypto
          metadata frp bcota phyboot0 phyboot0hwi phyboot1hwi …`
- BCB:   `perm` … `Could not mount perm` … `bootcontrolblock` … `Could not read
          BCB` / `Could not write BCB` / `ERROR: No misc p…`
- nvuser:`nvuser` … `Could not mount %s` … `NV %s is %s`

--------------------------------------------------------------------------------
## 3. On-device fastboot experiments (this session)

State: device in Android → `adb reboot bootloader` → fastboot enumerates.

| command | result |
|---|---|
| `getvar product` | `MSM8974` |
| `getvar serialno` | `1160724779` |
| `getvar max-download-size` | `0x20000000` |
| `getvar unlocked/secure/variant` | empty |
| `getvar all` | empty |
| `getvar partition-size:system` | `0xa0000000` |
| `getvar partition-size:userdata` | `0x6473fbe00` |
| `getvar partition-size:cache` | `0x40000000` |
| `getvar partition-size:boot/recovery/...` | empty |
| `oem info` | `FAILED authboot command permission denied` |
| `boot recovery.img` | `FAILED (remote: 'Not supported')` |
| `flash zzz junk.bin` | `FAILED authboot flash permission denied` |
| `flash recovery recovery.img` | `OKAY` (warn: "recovery partition size: 0") |
| `flash boot boot.img` (stock) | `OKAY` |
| `flash boot boot_mod.img` | `OKAY` (but **rejected at boot**) |

### 3a. HLOS verification proof
`boot_mod.img` = stock `boot.img` with only the 512-byte **cmdline** field
(offset `0x40`) changed from the stock 209-byte line to the same line plus
` androidboot.occamsux=1 androidboot.selinux=permissive` (263 B, fits).
`fastboot flash boot boot_mod.img` → OKAY; `fastboot reboot` → the device went
**straight back to fastboot** (no adb for >1 min) = LK's
*"Could not do normal boot. Reverting to fastboot mode."* Re-flashing the pristine
stock `boot.img` and rebooting → **Android came up normally**. ⇒ **LK verifies the
boot image; a cmdline-only change is enough to fail it.**

--------------------------------------------------------------------------------
## 4. Signature partition facts

```
bootsig        4096 B  ALL ZERO
recoverysig    4096 B  ALL ZERO
p_bootsig.bin  4096 B  ALL ZERO
p_recoverysig.bin 4096 B ALL ZERO
boot.img       33 550 336 B  sha256 64c43e8b…  (== recovery.img)
recovery.img   33 550 336 B  sha256 64c43e8b…
recovery_new.img 33 550 336 B sha256 4624c22e…
LOS recovery   15 226 880 B  sha256 8476149b…  (ANDROID!, kernel_size 5 464 864)
```

`bootsig`/`recoverysig` carry no data ⇒ LK's gate is **not** those partitions; it
is the **perm token** (`/perm/hlos_signature.tkn`) plus the
`/nvuser/hlos_unsigned.tkn` debug-token bypass (strings in §2/session62 §Unlock).

--------------------------------------------------------------------------------
## 5. Boot control block (BCB) — confirmed location and writer

`by-name/misc` open = `0xfffffffe` (ENOENT) ⇒ **no misc partition**.
`bkup_bc_update` (9 384 B ARM) strings give the full picture:

```
/dev/block/platform/msm_sdcc.1/by-name/bcota           (open O_RDWR, then setuid(2000))
  Dropping root to AID_SHELL
/nvram/perm                                             (FUSE; wait for it)
/nvram/perm/bootcontrolblock                            (open for read+write)
  Unable to read from the bcb block
  update-bootchain / update-new-bootchain
  There is already a pending bootchain update in the bootcontrolblock
ro.boot.binfo.bbss_wp_type   -> permanent / Backup bootchain is permanent write protected
ro.boot.binfo.backup_bc_ver  -> Current version:%s
  developer -> Developer backup bootchain in emmc. Skipping auto-upgrade
  jenkins   -> Old backup bootchain in emmc. forcing auto-upgrade
ro.boot.binfo.bbss_insecure  -> Security:%s
/system/vendor/backup_bootchain/backup_bootchain_%s.pkg  (%s = insecure | wp-power_on_secure)
  Header file is invalid (bad cookie)                   (magic 0x779D7ECE)
  Backup bootchain already up to date emmc=(%s), file=(%s)
  Wrote backup bootchain to staging partition (%llu B)  (-> bcota)
/nvram/perm/bootcontrolblock                            (open for writing)
  bcota / Unable to write the bcb block / Updated BCB
sys.powerctl = reboot,oem-0x81
```

⇒ The unlock does **not** need `misc`; it needs a write of the command
`update-bootchain` (or `update-new-bootchain`) into
`/nvram/perm/bootcontrolblock`, which is SELinux-guarded and currently unreachable
(no writable path from vold/shell; `perm` label `vtnvfs_block_device`).

`/system/vendor/backup_bootchain/` (recon copy):
`backup_bootchain_insecure.pkg` (AAA624), `backup_bootchain_wp-power_on_secure.pkg`
(**filename misleading — content is the insecure AAA624**), plus `parts_insecure/`,
`parts_secure/`, and `*.pkg.payload` (3 428 352 B).

--------------------------------------------------------------------------------
## 6. `/system` is dm-verity protected

`recon/passport-boot-dumps/ramdisk-text/fstab.qcom`:
```
/dev/block/platform/msm_sdcc.1/by-name/system  /system  ext4  ro,barrier=1,discard,trusted  verify=/system_signature,wait
/dev/block/platform/msm_sdcc.1/by-name/userdata /data  ext4  nosuid,nodev,... encryptable=footer,forceencrypt=.../metadata
```
⇒ swapping the vendor pkg in `/system` to force an update would break verity
(device would fail to boot the system). No `/vendor` mount — `/system/vendor` is
a directory inside `/system`.

--------------------------------------------------------------------------------
## 7. Blockers (unchanged / sharpened)

1. **HLOS image verification (LK)** — only exact-stock boot/recovery boot; a
   cmdline-only change is rejected. (Proven §3a.)
2. **authboot flash whitelist** — only `boot`/`recovery` flashable; bootloader
   partitions and `oem` commands denied.
3. **dm-verity on `/system`** — cannot stage a newer vendor pkg.
4. **SELinux** — `perm` (BCB) unwritable from vold/shell; `vtnvfsd`/
   `bkup_bc_update` unreadable (can't DC) and not startable (`ctl.*` inert).
5. **No `misc`** — BCB cannot be slipped in via a spare partition.
6. Debug token (`hlos_unsigned.tkn`) would bypass (1) but is not available and
   needs a write into `nvuser`.

--------------------------------------------------------------------------------
## 8. Next / open leads

- The quoted Discord note: *BBRY's prototype method starts Android from the
  **original unchangeable BB10 bootloader at LK state** (LittleKernel)* — i.e. the
  conversion lives in LK/BB10 side, not the Android bootchain. The build set
  "contains all of the tools to convert BB10-powered devices to Android".
  → investigate the conversion tools (`F:\Passport_Android_balika_v2.exe`,
  `F:\v2.0-android.signed`, `F:\BB_BL.zip`, `F:\imggen`, `F:\tools`) and how the
  BB10 LK chain-loads Android.
- `perm` filesystem format (LK "mounts perm") — reverse the on-disk
  `bootcontrolblock` offset/format; any chance to reach it from a permitted
  context.
- Re-examine LK fastboot for hidden commands that write the BCB
  (`bootchain update succeeded, rebooting…` VA ~0x101706E0).

--------------------------------------------------------------------------------
## 9. Files touched / produced
- `F:\FLASH-AFTER-REPAIR\*` (recovered vendor kit; README, .bat, LOS recovery+ROM).
- `Temp\opencode\boot_mod.img` (stock boot + cmdline marker; used to prove verify).
- `recon/passport-android-dump/partition-dumps/{bootsig,recoverysig,p_bootsig.bin,p_recoverysig.bin}`
  (all zero); `aboot.img` (string/partition tables, §2).
- `tools/session62-bcota-android/pulled/{bkup_bc_update,partitions_live.txt,bc_read_log.txt,cmdline_live.txt}`.
