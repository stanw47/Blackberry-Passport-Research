# Session 27 — Oslo prototype identity, the backup-boot-chain unlock, and kernel-LPE groundwork

Date: 2026-10-06/07. Builds on session22–26. Goal: research the Passport
prototype units and the Balika conversion path, decide whether the stock build
can be kept while running Android 11, and lay the groundwork for full kernel
root.

---

## 1. Prototype identity — this unit is an Oslo / Silver Edition prototype

Live props:

| prop | value |
|---|---|
| `ro.boot.binfo.product` | `oslo` |
| `ro.boot.binfo.name` | `oslorow` |
| `ro.build.fingerprint` | `unknown-1/oslorow/oslo:5.1/LMY47D/AAA787:user/release-keys` |
| `ro.build.display.id` | `AAA787 (master aospl-msm8974 576 qc8974_sfi-user)` |
| `ro.build.type` / tags | `user` / `release-keys` |
| `ro.boot.imagetype` | `sfi` (secure factory image) |
| `ro.boot.binfo.bbss_insecure` | `false` |
| `ro.boot.binfo.bbss_wp_type` | `power-on` |
| `ro.boot.binfo.hwid` | `0xf5b242c3` |
| `ro.boot.binfo.primary_bc_ver` / `backup_bc_ver` | `AAA787` / `AAA625` |
| `ro.boot.verity`, SELinux | `active`, enforcing |

The Silver Edition is codename **Oslo** (`SQW100-4`, released Aug 2015). The
Legacy Portable Computing Wiki prototype table lists "(no label) 5.1 Silver
Edition Passport — Apr 16, 2015" (and Mar 22, 2015) plus labeled units
(`AAA620` = "NOT FOR SALE" Black Passport, Mar 11 2015). Our unit has **no
label**, a Silver Edition shell, build AAA787 (Apr 5 2015) — an unlabeled
Oslo prototype, plausibly an executive/pre-release trial unit. It is a
**production-secure** build (user/release-keys, SFI, verity, enforcing), not
an eng/userdebug unit.

## 2. The prototype's real freedom — power-on WP, not an unlocked bootloader

- Retail Passports: **permanent** eMMC boot-partition WP → Balika's conversion
  needs an eMMC desolder + programmer.
- This unit: **`bbss_wp_type=power-on`** (confirmed by
  `backup_bootchain_wp-power_on_secure.pkg`) → boot partitions are writable
  once WP is lifted. Community: prototype Passports have unlocked bootloaders
  and need no desoldering.
- Our aboot is still the production chain with the authboot gate
  (`authboot command/flash permission denied`). The **engineering** aboot from
  the imggen kit has **no authboot gate strings** → that is the unlock.

## 3. imggen analysis (BBAndroids)

`imggen boot0 user new_boot0 new_user` (`main.py`):
- parses the source `boot0` build info (hwid/product/variant, `secure =
  build_info[8] == 0`, rev table, MCT), reads `nvram`, `cal_work`,
  `cal_backup` from `user`, regenerates HWI, then writes the **engineering
  boot chain** into a fresh boot GPT.
- `boot_gpt_insecure.bin`: hwi, bbss, sbl1r, tzr, rpmr, sdir, abootr.
- `boot_gpt_secure.bin`: same **plus stage1, stage2, stage3** (auth stages).
- `user_gpt.bin`: 40-partition conversion layout (gsign, mct_b, pad, nvram,
  calwork_b, dmi_b, calback_b, aboot, sbl1, rpm, tz, sdi, fsc, modemst1/2,
  blog, perm, nvuser, ddr, bkup_ddr, prdid(+sig), boardid(+sig), fsg, ssd,
  metadata, frp, bcota, rcause, spare, persist, crypto, ares, boot, recovery,
  modem, system, cache, userdata).
- The device's live by-name layout matches this almost exactly, plus
  `bootsig`, `oem`, `oemspare`.

**Not usable in place**: it needs the device `boot0` (unreadable, see §4) and
replaces the user GPT — it is a full conversion, not dual-boot.

## 4. boot0/boot1 are NOT exposed (negative)

- `/dev/block` has no `mmcblk0boot0/1`; `/sys/block/mmcblk0boot0` does not
  exist; dumps return `-1`. Direct in-place boot-chain flashing from the OS is
  impossible through the block interface.
- `bcota` (4 MiB staging/BCB partition) is **blank** — no pending update.
- Vold-domain root still cannot read the specific-labeled partitions
  (`nvram`, `calback_b`, `mct_b`, `prdid`, `boardid`, `modemst1/2`, `fsg`,
  `ssd`, `frp`, …); the generic ones dump fine.

## 5. The unlock path — the device's own backup boot chain

Boot-chain strings (`bbss`/`sbl1` dumps):
- **`Backup boot key combo active`** — a key combo boots the backup SBL.
- **`Failed to load primary SBL, will load backup SBL instead`** — fallback.
- `BACKUP`, `BBSS bootcount`, `backup_bootchain`, `backup_bc_ver`.

`bkup_bc_update` (`/system/bin/bkup_bc_update`, init service in `init.oem.rc`:
`class core`, `user root`, `group nvram mfg_stp`, `oneshot`,
`seclabel u:r:bkup_bc_update:s0`):
- reads `/system/vendor/backup_bootchain/backup_bootchain_%s.pkg`, where `%s`
  is chosen from `bbss_insecure` / `bbss_wp_type` (`insecure` vs
  `wp-power_on_secure`);
- compares the eMMC backup version (`ro.boot.binfo.backup_bc_ver`) with the
  package version: "already up to date" / "Old backup bootchain in emmc.
  forcing auto-upgrade" / "Developer backup bootchain in emmc. Skipping";
- stages the package into `/dev/block/platform/msm_sdcc.1/by-name/bcota`,
  updates the BCB, and **reboots to complete the update** — the bootloader
  then flashes the backup boot chain (WP handled by the boot chain);
- manual modes: `update-bootchain`, `update-new-bootchain`.

Packages on the device (`/system/vendor/backup_bootchain/`):

| pkg | version | certs |
|---|---|---|
| `backup_bootchain_insecure.pkg` | AAA624 | secure **and** insecure CA sets |
| `backup_bootchain_wp-power_on_secure.pkg` | AAA625 | secure CA set only |

**Unlock plan:** install the **insecure** package as the backup boot chain
(stage via `bkup_bc_update` or write the BCB/pkg directly), then hold the
backup key combo at power-on → insecure SBL/aboot (no authboot gate) →
`fastboot flash` a LineageOS recovery/ROM. Dual-boot is then possible via the
recovery slot / a custom GPT; storage is ample (SD 29.2 GB free, `/data`
15.3 GB free).

**Blocker:** the updater runs at boot and skips (AAA625 == AAA625); forcing it
with the insecure pkg requires either executing the root service (or
`ctl.start`) or staging the pkg/BCB directly — all need **full root**.

## 6. Kernel-LPE groundwork (full root)

- `vmlinux-to-elf` on the dumped zImage → symbolized `vmlinux.elf`;
  **no KASLR**, base `0xc0008000`; kallsyms at `0xa99580`.
- Symbols: `commit_creds=0xc01af0a4`, `prepare_kernel_cred=0xc01af570`,
  `init_task=0xc0f29890`, `init_cred=0xc0f5662c`, `ptmx_fops=0xc10a5fe4`,
  `sidtab=0xc107c2ac`, `policydb=0xc107c198`, `selinux_enabled=0xc0f647d8`,
  `pathtrust_enforce=0xc0f64f9c` (BB custom; **no `selinux_enforcing`**),
  `sys_call_table=0xc0106604`, `sys_ni_syscall=0xc01a9ee8`.
- **CVE-2015-1805** (pipe iovec overrun): kernel has the old vulnerable
  `pipe_iov_copy_from_user`; build date predates the fix/Android patch.
- iovyroot (dosomder) built for armeabi-v7a with NDK r23c; offsets entry added
  (`fsync = ptmx_fops+0x38 = 0xc10a601c`, verified against `vfs_fsync_range`).
- **Result**: the pipe race **works** — the arbitrary kernel write to
  `ptmx_fops.fsync` succeeded ("Patching address 0xc10a601c … Done"). The
  subsequent step (kernel calling the user-space `patchaddrlimit`) **panics** —
  same wall as the Priv: BB/grsec blocks kernel→user control flow
  (KERNEXEC-style). Two clean reboots, no data loss.
- **Pivot (kernel-only)**: ARM passes syscall args in registers (the table
  jump is `ldrlo pc, [r8, r7, lsl #2]`), so patch **unused** syscall slots
  (17 and 35, both `sys_ni_syscall`) to `prepare_kernel_cred` and
  `commit_creds`: `cred = syscall(17, 0)` → `syscall(35, cred)`. No user code,
  no addr_limit. `sys_call_table` is outside `__start_rodata` (0xc0a00000) and
  there is no `mark_rodata_ro` (only `set_all_modules_text_ro`) → writable.
  This patch was being built when this note was written.

## 7. Device usage notes (non-research)

- Debloat via `pm hide --user 0` (reversible): BBM, BBM Meetings, BlackBerry
  Safeguard (`privacydashboard`), BlackBerry Diagnostics (`ddt` + `.checkin`),
  Currents/Newsstand (`magazines`), Play Books/Games/Movies/Music,
  QC Logging (`com.qualcomm.RIDL`), Hangouts, Device Manager, plus
  `swperf.perfmon`, `batterylogger`, `avengers.retaildemo`.
- Battery: no single runaway wakelock; demo/logging trio hidden; the resident
  vold daemon polls 1/s (cleared by reboot).
- Play Store 39.7.34 and GMS are installed; Wi-Fi works; modern-app limits are
  the API-22 wall.

## 8. Current state / next steps

- `fsck_msdos` is patched (page cache) with the fixed vold daemon; it is
  resident and root. Device is otherwise stock.
- Next: build and run the syscall-table exploit for full root; then read
  `boot0`/secure partitions for the complete archive; then drive
  `bkup_bc_update` to install the insecure backup chain; hold the key combo;
  flash LineageOS (dual-boot via recovery slot/custom GPT).

## 9. Safety

Read-only except the volatile `fsck_msdos` page-cache patch. No flash writes.
The unlock step (backup-chain install) is the first persistent change and will
only be attempted once the full archive exists.
