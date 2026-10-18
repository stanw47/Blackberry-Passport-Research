# Session 71 — Building the kernel LPE for the oslo prototype (iovyroot, CVE-2015-1805)

Date 2026-10-18. Follows session27 (kernel-LPE groundwork). **Our work.** Base
tool is the public `dosomder/iovyroot` (CVE-2015-1805); our fork adds the oslo
prototype offsets and a new pivot. Source + built binary archived in-repo.

## 1. Verified the kernel work is the PROTOTYPE's
Extracted `vmlinux` from this device's `boot.img` (zImage, gzip payload @0x4698 →
16,578,904 B). `/proc/version` = `Linux version 3.4.0-grsec-g0ea3e38
(ec_agent@br605cnc) (gcc version 4.8 (GCC) ) #1 SMP PREEMPT Sun Apr 5 17:23:36
EDT 2015` — the exact running build. Base `0xc0008000`, no KASLR.
session27's addresses validate:
- `commit_creds=0xc01af0a4`, `prepare_kernel_cred=0xc01af570` (real prologues)
- `sys_call_table=0xc0106604` (indices 17 & 35 = `sys_ni_syscall` 0xc01a9ee8 ✓)
- `selinux_enabled=0xc0f647d8` = 1, `pathtrust_enforce=0xc0f64f9c` = 1
- `ptmx_fops=0xc10a5fe4` (fsync @ +0x38 = 0xc10a601c, matches the old iovy log)
- `sidtab=0xc107c2ac`, `policydb=0xc107c198`

## 2. Why the stock exploit's pivot fails here
- The CVE-2015-1805 pipe race gives an **arbitrary 4-byte kernel WRITE** (works —
  the old iovy.log wrote `ptmx_fops.fsync`).
- Stock 32-bit path: `ptmx_fops.fsync <- &patchaddrlimit` then `fsync(ptmx)`
  **executes userspace from kernel mode** → grsec **KERNEXEC** blocks it → panic
  (our iovy.log).
- The `sys_call_table`-patch variant no-ops: `sys_call_table` sits in grsec
  **read-only `.text`**, so the write doesn't land.
- `modprobe_path` string absent in this kernel → not a target. `uevent_helper` is
  an **empty writable `.data` char[] @ 0xc109ca40**.

## 3. Our fork
`tools/session71-iovyroot/` (Android.mk / Application.mk / main.c / getroot.c /
offsets.c / sid.c / flex_array.c + headers):
- `jni/offsets.c`: 32-bit entry for `unknown-1` + the kernelver above, with
  `FSYNC_OFFSET(0xc10a5fe4)`, sidtab 0xc107c2ac, policydb 0xc107c198,
  selinux_enabled 0xc0f647d8, selinux_enforcing 0x0, `uevent_helper 0xc109ca40`;
  `get_devname` reads `ro.product.brand=` (the device has no `ro.product.model`).
- `jni/main.c` (32-bit `getroot`): **writes `/data/local/tmp/rh` into
  `uevent_helper`** (4 B at a time via the race) then nudges a uevent — instead
  of the panicking userspace callback. Adds `<unistd.h>/<errno.h>/<fcntl.h>`.
- Build fixes: `-std=gnu99 -DPAGE_SIZE=4096 -fcommon`, headers, NDK r23c.

## 4. Build + test
- Built with the **already-installed NDK r23c** (`23.2.8568313`); needed a
  space-free junction (`C:\ndk`). Output `libs/armeabi-v7a/iovyroot` (15,712 B).
- Placed `/data/local/tmp/iovyroot2` + helper `/data/local/tmp/rh`
  (`#!/system/bin/sh` → writes `/data/local/tmp/rootproof.txt`, setenforce 0).
- **Runs as `shell`** (CVE-2015-1805 is an unprivileged local LPE — no vold root
  needed; `vold` can't exec `/data/local/tmp` anyway).
- Run #1: patched `0xc109ca40..0xc109ca50` ("Done" ×5), **no panic**.
- Trigger: `shell` can't write `/sys/**/uevent` (EACCES) but **vold can** (wrote
  with no error). However `/data/local/tmp/rootproof.txt` did **not** appear →
  the helper didn't run.
- Run #2 → **kernel panic** (the race is probabilistic and can corrupt the wrong
  address). User rebooted; device recovered.

## 5. Open questions / next
1. **Did the `uevent_helper` write land?** The "Done" only means the pipe
   overflow happened, not that the target write landed. Need a read-back
   (arbitrary READ). iovyroot's read path depends on `addr_limit = -1`, which the
   (panicking) callback provided — so add a non-panicking read (e.g. leak via a
   `.data` pointer) or verify indirectly.
2. **Reliability / safety:** the race can panic. Reduce failures (retry per
   address; verify; keep candidates RW `.data`).
3. **Helper exec:** confirm the kernel usermode-helper can exec
   `/data/local/tmp/rh` (SELinux/noexec) — else use a label the kernel can exec.
4. Alternative pivot if needed: patch a **RW function pointer** (fops/ops) to a
   kernel function, or find `modprobe_path`'s real address.

## 6. Safety
The exploit is a local LPE run as `shell`; it writes kernel `.data` via the race.
No flash writes. Risk = kernel panic (recovered by reboot). Archive first.
