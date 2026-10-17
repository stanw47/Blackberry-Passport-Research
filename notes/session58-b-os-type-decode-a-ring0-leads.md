# Session 58 — (B) RIMBOOT OS-type decode (no lead) + (A) ring0 leads

Date 2026-10-13. Device: retail Passport (Windermere EMEA, MSM8974AA, BB10/QNX,
uid-0 root). Follows session57 (RIMBOOT verification model). Goal (user): get the
boot chain to end up running Android (ideally a clean QNX→Linux handoff), without
desoldering.

---

## (B) RIMBOOT OS-type indirection — decoded, NO exploitable lead

`fcn.f8023318` dispatches on an OS type byte at `[0xf803b7a0]`:
- 0 = OS, 1 = installer OS, 2 = RFOS, 3 = RAMImage, else "unknown image".

`fcn.f8021ebe` SETS that type by parsing the loaded **OS image header**:
- RAMImage: `[img+4]==0xd7d32d1f` + `fcn.f8022436`/`fcn.f802136e` verify → type 3.
- RFOS: `[img+0x28]` magic `0xc3d0e193`, `[obj+0x9c]==0xc3d0e193`,
  `[obj+0xd4]==2`, `[obj+0x80] >= 0x30000e6` → type 2.
- else error (`fcn.f80219ec(0,0xbd,3,0x352)`).

⇒ The OS type is a property of a **valid signed image**; there is **no
writable-partition / weaker-check path**. And `os0` (the RIMBOOT-verified image)
**contains `procnto`** ("procnto Elf head", ELF @0x260a84) — so the kernel is
behind the signature wall.

**(B) conclusion: no lead.** The kernel is verified; every OS path needs a signed image.

## (A) Ring0 leads

### A1. `/dev/mem` — root-accessible, but tool load blocked
- `/dev/mem` = `-rw------- root nto` (root-only). Kernel = `procnto-smp-instr`.
- A QNX tool using `mmap`/`mmap_device_memory` fails to load: **"Can't access
  shared library"** — the device's `/proc/boot/libc.so.3` (598 KB) is
  **stripped** (`nm -D`: no symbols), and the loader refuses our imports. Only
  the symbols extcsd_probe uses (printf/open/close/devctl/setgid/exit/
  `_init_libc`) resolve. mmap-class symbols don't. (Fixable but fiddly.)

### A2. `/proc/<pid>/as` patching — WORKS as root (userspace)
From our own Classic session7t (verified there on a driver):
- open `/proc/<pid>/as` as root; **READ any vaddr** (`dd if=/proc/<pid>/as bs=1
  skip=<vaddr> count=N`); **WRITE `.data/.bss` WORKS**; **WRITE `.text` FAILS**
  (read-only page → "Server fault on msg pass").
- On the retail Passport, `/proc/1/as` (procnto) exists (root-only) but
  `/proc/1/mappings` is empty and a read at vaddr 0 returns 0 bytes — the
  **kernel's AS appears restricted** (the primitive is proven for userspace
  processes, not procnto).
- ⇒ We can patch a **userspace driver's writable data** (e.g. the SD/MMC
  driver), but **not kernel code**, and probably not the kernel at all.

### A3. The SD/MMC driver boot-WP flag (session7t) — ⛔ BRICKS THE BOARD — DO NOT DO
**DO NOT ATTEMPT.** The user confirms the CMD6 `BOOT_WP` clear patch **bricked the
old board** (the `11011` deadlock). The boot-partition WP is effectively
**permanent**; the session7t/7r "just set the attached flag and issue CMD6"
idea is a dead end that destroys the device. Recorded here so it is never retried.

(Original note kept for the record: session7t/7r decoded the driver's WRITE_PROTECT
→ `mmc_switch(BOOT_WP=0xad)` path gated by a per-partition "attached" flag; the
idea was to patch the flag and clear `B_PWR_WP_EN`. **It does not work and bricks
the eMMC. Closed.**)

## Update — tooling fixed; /proc/<pid>/as works on retail; kernel AS still gated

- **Tool-build bug FIXED:** building QNX tools under Git-Bash mangled the ELF
  `PT_INTERP` (`C:/Program Files/Git/usr/lib/ldqnx.so.2` instead of
  `/usr/lib/ldqnx.so.2`) → the loader errored "Can't access shared library".
  Fix: `export MSYS_NO_PATHCONV=1; MSYS2_ARG_CONV_EXCL="*"` before
  `arm-none-eabi-ld --dynamic-linker=/usr/lib/ldqnx.so.2`. Tools now load.
- **`/proc/<pid>/as` WORKS on the retail** (via `__root`): `asrw` read the SD/MMC
  driver's AS at VA `0x10b3d000` (ELF header, errno=0). `dd if=/proc/<pid>/as`
  also works but its `skip` is 32-bit (can't reach >2 GB).
- **`/proc/1/as` (procnto/kernel): opens but every read faults** —
  errno **312 = ESRVRFAULT ("Server fault on msg pass")**; a full 64-step scan of
  `0x0..0x100000000` found **no** readable vaddr. Classic sessions 11c/11d traced
  this to the process-manager **`PROCMGR_AID_MEM_PHYS`** ability (boot-dependent;
  `__root` requests it under `ADN_NONROOT` so it doesn't take effect for the
  kernel AS). "Whitelist" = obtain MEM_PHYS (e.g. a native tool that calls
  `procmgr_ability` with `ADN_ROOT`, or the historical SSH-as-root context).
- Net: we can read/write a **userspace** process's `.data/.bss`; we **cannot**
  reach the kernel's AS without MEM_PHYS. The one driver patch that mattered
  (CMD6 BOOT_WP) is a **brick** and is closed.

### Next (A) if pursued
1. Obtain `PROCMGR_AID_MEM_PHYS` (native `procmgr_ability(ADN_ROOT…)` tool) → read
   `/proc/1/as`, locate a **non-bricking** kernel target (e.g. a data/callout
   pointer to redirect) → kexec handoff.
2. Otherwise: route #2 (Android-on-QNX runtime).

### Update — MEM_PHYS granted, but /proc/1/as still faults
Built `memphys` (native, per Classic `tools/asroot_main.c` form):
`procmgr_ability(0, ADN_ROOT|AOP_INHERIT_YES|AOP_ALLOW|AOP_SUBRANGE|AID_MEM_PHYS,
0, ~0, AID_EOL)` → **returned 0 (grant OK)**; then `open /proc/1/as` = fd 3.
**Read still -1, errno 312 (ESRVRFAULT)** at 0x80000000 (and the earlier full scan).
⇒ The `MEM_PHYS` ability is not the (only) blocker: the `/proc` AS reader faults on
procnto's own AS regardless. Kernel memory is **not reachable via /proc/1/as**.
`/proc/<pid>/as` for **userspace** processes remains fully usable (driver read OK).

### ⭐ UPDATE — BREAKTHROUGH: /proc/1/as IS readable (the kernel is mapped)
The "blocked" call was wrong. `ESRVRFAULT (312)` simply means **unmapped vaddr**
(proved on a normal process: reads at its mapped code/lib/heap succeed; unmapped →
312). procnto's kernel image is mapped at a **high VA around `0xfe174000`**, which
the earlier coarse scans stepped over.

- Readable kernel region: **`0xfe174000` .. ~`0xfe218000`** (~672 KB) — this is
  procnto's code/rodata (mostly Thumb-2 code; ~46 strings only).
- Read it live with `asdump 1 0xfe174000 <len> <out>`; **no MEM_PHYS needed** —
  plain `asrw`/`asdump` under `__root` read it (errno 0).
- Dumped **`kern.bin`** (604,872 B, base `0xfe174000`) — first live kernel dump.

⇒ **(A) is ALIVE.** We can now read procnto. Next: enumerate the kernel's mappings
(find its writable `.data/.bss`), RE the syscall/dispatch tables, and find a
**non-destructive** ring0 target (e.g. a writable function pointer) to drive a
kexec handoff. (Writing to the kernel is the risky part — test on a *writable data*
page only, never code, and keep an undo.)

### ⭐⭐ UPDATE — KERNEL DATA IS WRITABLE; call table located
Pulled `/proc/boot/procnto-smp-instr` (696,320 B) — ELF32 ARM EXEC, stripped:
- **code** LOAD `0xfe173000` filesz `0x94ac8` (R E), entry `0xfe1a6518`
- **data** LOAD `0xfe208000` filesz `0x13870` (RW)
Wrote the same 8 bytes back at `0xfe208000` via `/proc/1/as`: **`write(8)=8`** ⇒
the kernel's RW data segment is **writable** from `__root`. Dumped `kdata.bin`
(~80 KB). In it: **986 function pointers** (data→code) starting densely at
`0xfe208000` (looks like the kernel **call/syscall table**), plus 411 data pointers.

⇒ Full **kernel-data R/W primitive**, no eMMC/WP/CMD6. Candidate ring0 paths:
(a) retarget a call-table entry to a kernel gadget / RWX page; (b) find/flip a
page-table entry to make a data page executable and drop ring0 shellcode → kexec;
(c) disable a security/ability check. **Next: confirm the call table's dispatch
site in `kern.bin`/code, then pick the least destructive target.**

## Where this leaves us

1. **(B) boot-chain handoff: closed** (kernel verified, no weaker OS path).
2. **(A) handoff (QNX→Linux kexec): needs ring0.** Kernel code is RO; `/proc/1/as`
   looks restricted; so the practical ring0 route is patching a **userspace
   driver's data** (A3) — which is actually about **clearing boot-WP and booting
   the balika Android chain**, not a kexec.
3. The **guaranteed** path to Android remains **Android-on-QNX** (keep the chain,
   run the runtime on the QNX kernel; shippable as a signed autoloader).

## Next (pick)
- **A-MEM_PHYS:** build a native tool that obtains `PROCMGR_AID_MEM_PHYS`, read
  `/proc/1/as`, find a non-bricking kernel target.
- **Route #2:** resume the A11-on-QNX runtime build.
- **(A3 / A1 are closed: A3 bricks; A1's `/dev/mem` path is dead.)**

## Artifacts
- `rimboot.bin` (extracted from `bb0_full.bin`); r2 decode; function VAs above.
- `memtool/` (QNX /dev/mem tool source + build recipe; loads-blocked).
- `os0_8M.bin` (verified QNX image: startup + procnto + core).
