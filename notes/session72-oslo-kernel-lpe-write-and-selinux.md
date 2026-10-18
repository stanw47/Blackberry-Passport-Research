# Session 72 — oslo kernel LPE: write primitive proven, SELinux untouchable, next = read/cred

Date 2026-10-18. Our work. Follows session71 (iovyroot fork). Continues toward full
kernel root on the oslo prototype so we can reach the eMMC boot area.

## 1. Arbitrary kernel WRITE: PROVEN and reliable
CVE-2015-1805 race, run as `shell` (no vold root needed). Verified live by
flipping a value and reading it back through an existing interface:
- `pathtrust_enforce` (0xc0f64f9c) 1 → 0 — read back as **0** via
  `/sys/kernel/security/pathtrust/enforce` (needs root to read).
- `selinux_enabled` (0xc0f647d8) 1 → 0 (no visible effect — see §3).
So `write_at_address(target, val)` lands 4 bytes at the target reliably.

## 2. Full symbol map recovered (base 0xc0008000, no KASLR)
The zImage's gzip payload (file 0x4698) decompresses to the vmlinux (16,578,904 B).
`kallsyms_addresses` is at **file 0xa99580**; `addrs[0]=0xc0008000` ("stext") and
it lines up 1:1 with the device `/proc/kallsyms` name order (89,247 syms;
1,901 duplicate-address entries). Key symbols:

| symbol | addr |
|---|---|
| sys_call_table | 0xc0106604 |
| commit_creds | 0xc01af0a4 |
| prepare_kernel_cred | 0xc01af570 |
| ptmx_fops | 0xc10a5fe4 (fsync +0x38 = 0xc10a601c) |
| init_task / init_cred | 0xc0f29890 / 0xc0f5662c |
| selinux_ops / selinux_enabled | 0xc0f1bf2c / 0xc0f647d8 |
| pathtrust_enforce | 0xc0f64f9c |
| ss_initialized | 0xc107c298 |
| sel_enforce_ops | 0xc0a28d8c (struct file_operations) |
| kptr_restrict | 0xc0f1c220 |
| sidtab / policydb | 0xc107c2ac / 0xc107c198 |
| **uevent_helper** | **0xc109ca40** (confirmed) |

## 3. SELinux is permanently enforcing on this build
- `sel_enforce_ops` (`selinuxfs` "enforce" file) has **`.write = NULL`** → the file
  is **read-only**; that is why `setenforce 0` ⇒ "Could not set enforcing status:
  Permission denied". There is **no `selinux_enforcing` symbol** (the runtime flag
  is a file-static), so it can't be flipped by name.
- Zeroing `selinux_enabled` (the boot-time registration toggle) does **not**
  unconfine (hooks are already registered).
⇒ SELinux will stay enforcing; we must escalate **through** it, not around it.

## 4. Dead end: the usermode-helper pivot panics
- Stock iovyroot's pivot (`ptmx_fops.fsync <- userspace patchaddrlimit`) panics
  (grsec KERNEXEC), and `sys_call_table` is grsec read-only `.text`.
- Our `uevent_helper` `.data` pivot writes fine (address confirmed) but the kernel
  `call_usermodehelper` path — especially pointing it at a Dirty-COWed
  `/system/bin/applypatch` — **panicked** the kernel (twice), so it is abandoned.

## 5. Plan: READ → patch our own cred (uid0 + caps + sid=init)
Patching the current task's `cred` to `uid 0`, full caps, and SELinux
`sid = init` gives root **and** init's unconfined context — no need to disable
SELinux. The missing piece is a **read** to locate our `task_struct`/`cred`.

**Read primitive to build:** use our write to repoint a **`/proc/sys` `ctl_table`
`data`** pointer at an arbitrary kernel address, then read that sysctl file →
leaks kernel memory (4 B at a time; `proc_dointvec`). Walk to find our
`task_struct` (scan for the cred pattern like stock `modify_task_cred_uc`) then
patch `uid/gid/euid/egid/fsuid/fsgid → 0`, `cap_* → 0xffffffff`, and
`security->sid/osid → init` (via `sidtab`/`policydb`, using `get_sid("init")`).

## 6. Artifacts
`tools/session71-iovyroot/` (fork: our 32-bit offset entry for `unknown-1` +
the kernelver, `get_devname` reads `ro.product.brand=`, uevent/pathtrust adds,
build fixes: `-std=gnu99 -DPAGE_SIZE=4096 -fcommon`, NDK r23c). Custom build
runs as `shell`; writes confirmed.

## 7. Safety
Local LPE as `shell`; writes kernel `.data` via the race. Risk = kernel panic
(recovered by reboot). No flash writes. Keep archiving.
