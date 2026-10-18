# Session 73 — Unconfined init-domain root via init keychord + arbitrary eMMC access

## Summary
Found and proved a **code-execution primitive that lands in the SELinux `init` domain
(`u:r:init:s0`) — unconfined root** — on the BlackBerry Passport Android prototype ("oslo",
AAA787). From there, obtained **raw read/write of every eMMC partition**, MMC_IOC_CMD to the
eMMC, a reliable **exfil channel**, and confirmed **the boot chain boots from the user area**.

## 1. The escalation primitive (no DARK / no prior root needed)
`init` starts a service **without any `seclabel`** in the `init` domain. init even logs it:
`init: Warning! Service bugreport needs a SELinux domain defined; please fix!`.

Reachable trigger without any property permission:
- `service bugreport /system/bin/bugreport_service` — `class main`, `disabled`, `oneshot`,
  **`keycodes 114 115`**, **no seclabel** → runs in `u:r:init:s0`.
- init listens for the key-chord on `/dev/input/event4` (the `keychord` input handler; event4 =
  `gpio-keys-oem`).
- `shell` is a member of group **`input`** (`id` → groups=...,1004(input),...) so it can write
  `/dev/input/event*` (`sendevent` -> toolbox; no avc denial).

So, entirely from `adb shell`:
1. Dirty-COW (`ptracecow2`, session26) the page cache of `/system/bin/bugreport_service`
   (511 bytes, `u:object_r:system_file:s0`, mode 0644 → shell-readable) with our payload.
   The kernel picks the interpreter from the *current file content*, so the payload is simply a
   `#!/system/bin/sh` script (≤ 511 B) — original file type is irrelevant.
2. Inject the chord on event4:
   `sendevent /dev/input/event4 1 114 1; sendevent ... 1 115 1; sendevent ... 0 0 0` (+ release).
3. init starts `bugreport` → executes our script **as `u:r:init:s0`, uid 0**.
Proof: `[BP] u:r:init:s0`.

## 2. Chained arbitrary code (bigger payloads + ELF)
The init-domain script can set properties (`setprop ctl.start ...`) — init owns `ctl_default_prop`.
`service profiler_daemon /system/bin/profiler_daemon` (13712 bytes, `system_file`, **no seclabel**,
`class late_start`, `user root`, not oneshot) →
1. Dirty-COW `/system/bin/profiler_daemon` with up to **13712 bytes** (script **or ELF**).
2. From the bugreport payload: `setprop ctl.start profiler_daemon`.
3. init execs our payload in `u:r:init:s0`.
Confirmed: `[PD] ctx=u:r:init:s0`, `init: Warning! Service profiler_daemon needs a SELinux domain defined`.

## 3. init-domain capabilities (measured)
- Writes OK: `/data`, `/data/local`, `/data/misc`, `/cache`, `/dev`. NOT `/data/local/tmp`
  (`shell_data_file`), NOT `/storage`, `/mnt`.
- Exec OK: only `/system` (`system_file`). Exec/relabel denied for `system_data_file`,
  `cache_file`, `shell_data_file`; `chcon ... system_file` denied (`relabelto`).
  → bootstrap ELF via the profiler_daemon page-cache (above), not via exec of dropped files.
- **Raw block I/O:** `open("/dev/block/mmcblk0", O_RDWR)` OK (`ro=0`); read + **write of partitions
  OK** (`dd ... of=/dev/block/mmcblk0p30` → rc=0). Whole-device `mmcblk0` write is the only quirk.
- **MMC_IOC_CMD (`0xc048b300`) works** from init domain on the whole device (CMD13 rc=0).
- Exfil: **vold** (session62 root daemon) CAN read `/data/local` and copy to `/sdcard` even though
  `shell` cannot (`opendir /data/local: Permission denied`).

## 4. Partition map (from init domain, `ls -l /dev/block/bootdevice/by-name`)
gsign=p1 mct_b=p2 nvram=p4 calback_b=p7 **aboot=p8 sbl1=p9 rpm=p10 tz=p11 sdi=p12** fsc=p13
modemst1=p14 modemst2=p15 blog=p16 **perm=p17** nvuser=p18 bkup_ddr=p20 boardid=p23 boardidsig=p24
fsg=p25 ssd=p26 metadata=p27 bcota=p29 rcause=p30 spare=p31 ares=p34 oem=p35 oemspare=p36
**boot=p37 bootsig=p38 recovery=p39 recoverysig=p40 modem=p41 system=p42 cache=p43 userdata=p44**

## 5. eMMC EXT_CSD (CMD8 SEND_EXT_CSD) — key fields
- **PARTITION_CONFIG (179) = 0x10** → bits[2:0]=0 ⇒ `BOOT_PARTITION_ENABLE=0`
  ⇒ **boot chain comes from the USER AREA**, not the boot partition.
  bits[5:3]=2 (current PARTITION_ACCESS=boot1, kernel-managed).
- BOOT_BUS_WIDTH (177)=0x04, BOOT_SIZE_MULT (226)=0x20 (32*128K = 4 MiB boot parts),
  CSD_STRUCTURE (194)=2 (eMMC>=4.5), EXT_CSD_REV (192)=7 (1.7).

## 6. Consequence / next
Because the chain boots from the user area, `sbl1`(p9)/`aboot`(p8)/`tz`/`rpm` are writable **directly**
with `dd` from init domain — no eMMC boot-partition switch required. Remaining question is the
signature chain (PBL→SBL1→LK→kernel). LK-verifies-HLOS was previously proven; whether PBL→SBL1 /
SBL1→LK are fused-verified on this unit is untested. Any boot-chain WRITE risks a non-booting unit;
boot-area (boot0/boot1) access additionally risks a *persistent* EXT_CSD PARTITION_ACCESS change.
Backups of p8/p9/p10/p11/boot/recovery exist in `recon/`; boot-area (4 MiB x2) not yet dumped.

## 7. Boot-chain ground truth (from the dumped `blog` = p16 boot log)
The `blog` partition carried the full chain logs. The flow is:

```
BBSS (Version AAA625)            <- boot area
  MMC init; Boot partition: 2 ; product=oslo variant=row pcb_rev=2 pop_rev=4
  Loading PRIMARY SBL ; Authenticating SBL ; Sig info: ec_agent, 20150405.172215, APBC
SBL1 (sbllog)                    <- boot area sbl1r
  Image Load, Start ; WDT Image Loaded ; Image Load, Start ;
  APPSBL Image Loaded, Delta ; Secure fusemap configured ; MSMID: 0x107BC0E1 ;
  is_processor_bound: TRUE ; Processor bound and verified ; SBL1, End
LK (lklog)                       <- APPSBL
  welcome to lk ... use_signed_kernel=1, is_unlocked=0, is_tampered=0.
  Loading boot image (11100160): start/done
  Authenticating boot image (11100160): start
    hlos_unsigned.tkn not found -> "Signature not found!" -> sig 0 return value = 0
    "Debug Token (hlos_signature.tkn) parsed successfully"
    "Attempting HLOS image verification with \"ADBI\" key" -> sig 1 return value = 1  (PASS)
  Build number: AAA787 ; Image type : SFI ; BOOT IMG build variant is user
  ... "OS passed blacklist check" ; "Bootchain fuse validation disabled in blocklist"
  ... "No label or valid binfo_data" (x3)
```

So: **the running chain is the boot-area one** (`BBSS -> sbl1r -> APPSBL`); the
user-area `aboot`/`sbl1` are a separate copy. `Boot partition: 2` matches the live
EXT_CSD `PARTITION_ACCESS=2` (boot1).

## 8. Why HLOS verification currently passes — and the token wall
- `/perm/hlos_signature.tkn` (dumped from p17) contains a vtnvfs debug token:
  `{state:"prerelease",key_name:"ADBI"}` with companion `.sig.hlos_signature.tkn`.
  This *prerelease* token makes the LK verify the HLOS with the **debug key "ADBI"**
  (the shipping `boot.img` carries the matching signature — sig 1).
- The LK looks for a separate `/nvuser|perm/hlos_unsigned.tkn` to **skip verification**
  (`Debug token hlos_unsigned is active. Skipping image verification.`).
- vtnvfs per-file signatures are **name-bound** — the `.sig.hlos_signature.tkn` blob
  literally embeds `hlos_signature.tkn` (`@0x1678`), so copying/renaming the token
  breaks its signature. Without BB's signing key the token route is closed.

## 9. Consequence for LineageOS
Every reachable route to boot a custom kernel requires landing BlackBerry's
**insecure/engineering boot chain in the eMMC boot area** (`boot0/boot1`):
`parts_insecure/{bbss,sbl1r,tzr,rpmr,sdir,abootr}.img` (+ engineering `aboot.mbn`).
The user-area `aboot` write alone cannot change the authenticated (boot-area) chain.
Writing the boot area needs `MMC_IOC_CMD` PARTITION_ACCESS switching (the tool above
proves MMC_IOC_CMD works from init domain). Risk: `PARTITION_CONFIG` (byte 179) is
non-volatile — preserve `BOOT_PARTITION_ENABLE` bits[2:0]=0 and always restore
`PARTITION_ACCESS`; a mistake can leave the unit unbootable.

## 10. Boot-area (boot0/boot1) is NOT reachable from the running OS
An init-domain tool (`baredump.c`) issued `CMD6 SWITCH` (`0x03B30000|(v<<8)`, the
exact `mmc_switch` encoding) to set `PARTITION_ACCESS`=1 then 2, read the 4 MiB boot
partitions with `CMD17`, and restored the original `PARTITION_CONFIG`. Result:
- `CMD13`/`CMD8`/`CMD17` on the whole device all succeed (`MMC_IOC_CMD` works).
- But immediately re-reading EXT_CSD byte 179 (`batest.c`) gives `0x10` **unchanged**
  after both set attempts → the switch does not take.
- Consequently `boot0.img` and `boot1.img` came out **byte-identical** and contain the
  **user-area GPT** (`gsign,mct_b,pad,nvram,calwork_b,dmi_b,calback_b,aboot,sbl1,rpm,
  tz,sdi,…`; `aboot` at LBA 262144 = 128 MiB, impossible inside a 4 MiB boot part).
- The kit's `parts_secure/abootr.img` is **not** present in either read.

⇒ The eMMC boot partitions need the kernel's full partition-switch (incl. the
re-init it performs) or the BootROM/official-updater lane — consistent with the
original assumption that the **boot area is only writable via the RAM-loader lane**.
The user-area chain (p8/p9) remains fully writable from init domain, but the live
chain is the boot-area one (`BBSS→sbl1r→APPSBL`), so a user-area write cannot
change the authenticated boot chain.

Backups: `recon/passport-android-dump/emmc-full-backup/` (42 user-area partitions,
3.06 GB) and `.../bootarea/{boot0.img,boot1.img}` (both = user-area content).

## 11. The unlock lane (official-updater / `update-bootchain`) — now armed
Session62 mapped this lane; with init-domain root it becomes driveable:
- **BCB** = a **0x440-byte block** kept as the vtnvfs file `bootcontrolblock` in
  **`perm`** (p17) — init domain can write it. Currently **all-zero** (no update pending).
- **`bcota`** (p29) currently holds a bootchain pkg, **version `AAA626`**
  (header magic `ce7e9d77`, size `0x00345038`, AAA626), whose fingerprints match
  **`backup_bootchain_insecure.pkg`** (`insecure device; ignoring` ×1; CA counts
  12/8/7) ⇒ the staged chain is the **INSECURE** one, and AAA626 **>** the installed
  backup area `AAA625` ⇒ the session62 "newer-only" gate now passes.
- **LK** contains the updater: `update-bootchain` / `update-new-bootchain`,
  `Reading BC update from '%s'`, `Primary BC: images=%d size=%d`, `Updating backup
  bootchain (%d B)`, `phyboot1`, `bootchain update succeeded, rebooting...`.
  It flashes the staged pkg into **phyboot1** (the *backup* boot area); phyboot0
  (primary/secure) is untouched.
- **Trigger** (community `unlock.sh`): stage pkg → `setprop sys.powerctl
  reboot,oem-0x81` → LK updates phyboot1 → power on holding the **backup-boot key
  combo** → BBSS loads the insecure backup SBL → insecure LK (tolerates HLOS-verify
  failure) → flash/boot the LineageOS recovery/ROM.

Risk profile: writes only the **backup** boot area; primary (phyboot0) stays secure,
so a failed/odd update should remain bootable via the normal path. Remaining work:
confirm the exact trigger (reverse `bkup_bc_update`'s version gate + BCB write) and
the backup-boot key combo before committing.

## Files
- `tools/session26/ptracecow2.c` — page-cache overwrite primitive (used from `shell`).
- `tools/session62-bcota-android/root_daemon.c` — vold-domain root (read /data/local, exfil).
- temp: `mmcdump.c` (EXT_CSD reader run as profiler_daemon), `bp.sh`/`pd.sh` payloads.
