# Session 74 — The boot-chain unlock lane (bkup_bc_update → LK update-bootchain → phyboot1)

Date 2026-10-18. Follows session73 (init-domain root). Device: oslo Android
prototype (AAA787), currently `bbss_insecure=false`, `bbss_wp_type=power-on`,
`primary_bc_ver=AAA787`, `backup_bc_ver=AAA625`, booting the **primary** boot-area
chain (BBSS→sbl1r→abootr).

## 0. TL;DR
The no-desolder boot-area write is **OS-side after all**, via the vendor's own
backup-bootchain updater. With **init-domain root** (session73) we can:
1. make `bkup_bc_update` see a **newer** backup pkg (patch the `/system` secure
   pkg's page cache with the *insecure* pkg — both are world-readable system_file);
2. let `bkup_bc_update` stage it into `bcota` and write the **BCB**
   (`/nvram/perm/bootcontrolblock`, 1088 B) and set `sys.powerctl=reboot,oem-0x81`;
3. on reboot the **LK `update-bootchain`** flashes the staged pkg into **phyboot1**
   (the *backup* boot area; **phyboot0 / primary secure chain untouched**);
4. power on holding the **backup-boot key combo** → BBSS loads the insecure backup
   SBL → insecure LK that tolerates HLOS-verify failure → flash/boot LineageOS.

## 1. `bkup_bc_update` reverse-engineering (`/system/bin/bkup_bc_update`, 9384 B, Thumb)
Strings (`.rodata`, file `0xee8`):
```
insecure | wp-power_on_secure | bkup_bc_update
Starting backup bootchain update check
/dev/block/platform/msm_sdcc.1/by-name/bcota
Dropping root to AID_SHELL ; /nvram/perm ; Waiting for nvram
/nvram/perm/bootcontrolblock
Unable to open /nvram/perm/bootcontrolblock for reading and writing. Error=(%s)
Unable to read from the bcb block
update-bootchain | update-new-bootchain
There is already a pending bootchain update in the bootcontrolblock
ro.boot.binfo.bbss_wp_type | UNKNOWN | Write protect:%s
permanent | Backup bootchain is permanent write protected
ro.boot.binfo.backup_bc_ver | Current version:%s
developer | Developer backup bootchain in emmc. Skipping auto-upgrade
jenkins   | Old backup bootchain in emmc. forcing auto-upgrade
ro.boot.binfo.bbss_insecure | Security:%s
/system/vendor/backup_bootchain/backup_bootchain_%s.pkg
Header file is invalid (bad cookie)
Backup bootchain already up to date emmc=(%s), file=(%s)
Backup bootchain will be updated emmc=(%s), file=(%s)
Bootchain file size is larger than available space in staging partition.
Wrote backup bootchain to staging partition (%llu B)
Unable to open /nvram/perm/bootcontrolblock for writing
bcota | Updated BCB | Unable to write the bcb block
Rebooting to complete backup bootchain update
sys.powerctl | reboot,oem-0x81
```
Flow (Thumb): open `bcota` (O_RDWR) → `setuid(2000)` (AID_SHELL) → wait for
`/nvram/perm` → open+`read(1088)` `/nvram/perm/bootcontrolblock`; if the command is
already `update-bootchain`/`update-new-bootchain` → *already pending*, exit → read
`ro.boot.binfo.bbss_wp_type` (if `permanent` → *permanent write protected*, exit) →
`ro.boot.binfo.backup_bc_ver` → pick variant from `bbss_insecure`
(`insecure` vs `wp-power_on_secure`) → open `/system/vendor/backup_bootchain/
backup_bootchain_%s.pkg`, validate header cookie, **version-compare** → copy pkg to
`bcota` via `sendfile` → build the BCB: `memset(buf,0,1088)`, copy a 20-byte
command (`update-new-bootchain`), set 6 bytes at `buf+0x40/0x44`, `write(fd,buf,1088)`
→ `property_set("sys.powerctl","reboot,oem-0x81")` → `pause()`.
Pkg header: magic `0x779D7ECE`, size `0x00345038`, hdrver 1, version `AAA6xx`.

## 2. Live state (read-only, this session)
- `bcota`(p29) already contains the **insecure** pkg, version **AAA626**
  (header `ce7e9d77 38503400 01000000 cacea9d2 41414136 3236`), fingerprints =
  `backup_bootchain_insecure.pkg` (`insecure device; ignoring` ×1; CA 12/8/7) —
  so the insecure chain is **staged but not committed**.
- `/nvram/perm/bootcontrolblock` (from `perm.img`) is **all-zero** → no pending update.
- `ro.boot.binfo.*` = AAA787 / AAA625 / false / power-on → the *automatic* path
  selects the **secure** variant (AAA625 == emmc) and does nothing ⇒ that is why the
  staged AAA626 has never been committed.

## 3. Trigger plan (vendor-faithful)
1. `ptracecow_big /system/vendor/backup_bootchain/backup_bootchain_wp-power_on_secure.pkg
   /system/vendor/backup_bootchain/backup_bootchain_insecure.pkg` (run as `shell`;
   both files are system_file 0644 ⇒ readable; CAP=5 MB covers 3.4 MB) — makes the
   *secure* variant file carry the *insecure* payload (AAA626).
2. From init domain: `setprop ctl.start bkup_bc_update` (or restart) ⇒ it reads the
   patched file, sees file AAA626 > emmc AAA625 ⇒ **"will be updated"** ⇒ writes
   `bcota` + the BCB + `sys.powerctl=reboot,oem-0x81`.
3. Reboot: LK sees the BCB command ⇒ `update-bootchain` ⇒ flashes the staged pkg
   into **phyboot1**.
4. Power on holding the **backup-boot key combo** ⇒ BBSS loads the insecure backup
   SBL ⇒ insecure LK (no HLOS gate) ⇒ flash/boot LineageOS.

## 4. Risks / rollback
- Only the **backup** boot area (phyboot1) is written; **phyboot0 (primary/secure)
  is untouched**, so a normal power-on should still boot the stock chain.
- `PARTITION_CONFIG` byte 179 currently `0x10` (access=boot1) — the LK handles the
  actual boot-area switching (we do not switch it ourselves; session73 §10 showed a
  bare `CMD6 PARTITION_ACCESS` from the OS does not take).
- Dirty-COW on a 3.4 MB file is the least reliable part; a partial patch ⇒ LK `CRC
  failure` ⇒ `Backup bootchain update failed`, phyboot1 (hopefully) unchanged.
- Backups: `recon/passport-android-dump/emmc-full-backup/` (42 partitions incl.
  `perm`, `bcota`) and `.../bootarea/`.

## Files
- `tools/session62-bcota-android/pulled/bkup_bc_update` (+ `bkup_bc.dis.txt`),
  `.../root_daemon*`, `.../bcota_read*`.
- `tools/session26/ptracecow_big` (CAP=5 MB page-cache overwrite).
