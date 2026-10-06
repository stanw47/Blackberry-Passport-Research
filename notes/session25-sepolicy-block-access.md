# Session 25 — SELinux policy parsed: which domains can touch block devices

Date: 2026-10-06. Follow-up to session24 (diagnostics exec + Dirty COW). Goal:
answer "which root process re-exec target is worth Dirty-COWing" from the
actual policy, not guesswork.

---

## 1. The dumped `sepolicy` was corrupt — re-pulled live

`recon/passport-android-dump/root/sepolicy` (216692 bytes) failed to parse with
libsepol. Byte analysis showed a **CR→CRLF expansion** (`0x0d` → `0x0d 0x0a`,
1337 occurrences) introduced by the original dump (likely `adb shell cat`
through a pty). Re-pulled binary-safe:

```
adb pull /sepolicy   ->  215355 bytes  (this is the correct file; committed)
```

## 2. Parsing pipeline (no setools for v26)

Kernel policy is **version 26**; setools 4.5.1 rejects <29. Built a small
libsepol dumper instead:

- libsepol 3.8.1 source built from the Debian orig tarball
  (`libsepol_3.8.1.orig.tar.gz`).
- `tools/session25/dump_cil.c` — `sepol_policydb_read()` +
  `sepol_kernel_policydb_to_cil()` → CIL text.
- Result: **policy v26, 808 types, 86 classes**, `policy.cil.gz` (72 KB).

## 3. Block-device access (from CIL `(allow ...)` rules)

Targets are all `dev_type` subtypes; `block_device` is the generic by-name
partition type.

| domain | target:class | perms |
|---|---|---|
| **rmt_storage** | `block_device:dir` | ioctl read getattr search open |
| **rmt_storage** | `mmc_block_device:blk_file` | ioctl read getattr lock open |
| **vold** | `block_device:blk_file` | ioctl read write create getattr setattr lock append unlink link rename open |
| **vold** | `block_device:dir` | full rwx + add/remove_name |
| **nvramd** | `block_device:dir` search; `nvram_block_device:blk_file` | read write |
| **reset_cause** | `block_device:blk_file` | read write open (+ dir search) |
| **systemrdumpsvc** | `block_device:blk_file` | read write open (+ dir search) |
| **uncrypt** | `block_device:blk_file` | write append open (no read) |
| **install_recovery** | `block_device:blk_file` | full rw |
| **bkup_bc_update** | `block_device:blk_file` | full rw |
| kernel / unconfineddomain | all block types | full |
| system_server | `frp_block_device` | rw |
| mdm_helper, qcomsysd, vtnvfsd, tee | `block_device:dir` | search (+open) |

Live daemon facts: `vold` runs **root**; `rmt_storage` runs **nobody** (domain
still has mmc read); `reset_cause` runs as its own uid; `subsystem_ramdump`
runs **root**.

## 4. Diagnostics (`ddt`) domain limits (from CIL)

`diagnostics` can: rw `diagnostics_data_file` + `dalvikcache_data_file`,
read `shell_data_file:file`, set only `bbdiag_prop`, connect to `init`
(property socket) + `token_service`, binder-transfer to `reset_cause`,
`rild`, `subsystem_ramdump`, and read the kernel syslog. It has **no**
`/dev/block` access, **no** socket access to vold/rmt_storage/nvramd, and no
`ctl.start`. Nobody can `signal` rmt_storage or vold except `init`
(transitions) / `debuggerd` (ptrace).

## 5. Escalation options (ranked)

1. **Physical trigger + Dirty COW helper** — Dirty-COW a vold helper
   (`fsck_msdos`/`sgdisk`/`e2fsck`), insert a microSD card into the Passport
   slot; vold (root) execs the helper → payload runs with block access and can
   write the dump to the card. Needs the user to insert a card.
2. **Reverse `subsystem_ramdump`** (root, block rw) — `ddt` can binder-transfer
   to it; if its interface can trigger a ramdump/read to a reachable path,
   partitions may be dumpable without root.
3. **Patch running daemon text via Dirty COW** — vold is root with full block
   access and its text pages *are* the page-cache pages Dirty COW writes;
   ARM I-cache coherency makes this unreliable but worth a test.
4. **Boot-time domains** (`install_recovery`, `bkup_bc_update`, `uncrypt`) —
   page-cache writes do not survive reboot, so these are not directly usable.

## 6. Artifacts
- `recon/passport-android-dump/root/sepolicy` — corrected live policy (215355 B).
- `recon/passport-android-dump/sepolicy-analysis/policy.cil.gz` — full CIL.
- `tools/session25/dump_cil.c`, `dump_av.c` — parsers.
- This note; `session24-hardening-and-dirtycow.md` (Dirty COW primitive).
