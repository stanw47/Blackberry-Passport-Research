# Session 60 — Android on the retail Passport: **ring0 achieved**, kexec-handoff status

Date 2026-10-13. Retail Passport (SQW100-1, MSM8974AA, BB10/QNX 6.5, Krait
ARMv7). Follows sessions 57–59. **Headline: we now have arbitrary ring0 code
execution on the QNX kernel — entirely in software, boot chain untouched.**

------------------------------------------------------------------------------
## 0. Goal
Boot **balika LineageOS 18.1 (Android 11, Linux kernel)** on the retail Passport
with the **signed RIMBOOT/QNX boot chain left intact** (no boot0 write, no
desolder, no CMD6/WP — those brick). Deliverable: an installable (autoloader /
uid-0) — running **one OS at a time** (QNX only long enough to hand off).

------------------------------------------------------------------------------
## 1. The primitive chain we built
1. **uid-0** via `pathtrust`/`__root`.
2. **`/proc/1/as` read+write** on procnto:
   - code `0xfe173000`–`0xfe207ac8` (RO), **data `0xfe208000`–`0xfe219870` (RW)**.
   - `ESRVRFAULT(312)` = "unmapped vaddr" (not a permission wall).
3. **Kernel call dispatch table** at **`0xfe211378`** (106 entries), dispatched by
   `ldr pc,[r2,ip,lsl #2]` @ `0xfe1aab5c`; call # in **r12**.
4. **Userspace trigger convention:** `mov r12,#N; svc #0x51`.
5. **RING0 PROOF:** redirected call **#105** (`__KER_POWER_SPARE4`, slot
   `0xfe21151c`, orig `0xfe1e057c`) → our `.text` payload → `svc #0x51` → the kernel
   `blx`'d our **user page** and ran it at **ring0** (wrote `0x0BADF00D` into kernel
   cell `0xfe218000`) → restored. **Arbitrary ring0 execution, reversible.**

Why it works: ARMv7 Krait — **no PAN/SMEP**; a kernel `blx` to a user page runs in
supervisor mode. (PXN would have blocked it; it doesn't.)

**Ring0 readback (CP15):** SCTLR=`0x10c5787d` (MMU+I/D+high-vectors),
TTBR0=`0x0141202b` (tables @ phys `0x01412000`, low 2 GB),
TTBR1=`0x00e1c02b` (tables @ phys `0x00e1c000`, high 2 GB = kernel),
TTBCR=`1` (split @ 0x80000000), MPIDR=`0x80000001` (core 1).

------------------------------------------------------------------------------
## 2. Phase 2 (kexec) — targets are ready
From `F:\FLASH-AFTER-REPAIR\lineage-18.1-...wseries.zip` → `boot.img`:
- kernel `Image` (uncompressed, ARM head) → load **phys `0x8000`**, size `0x517F88`.
- ramdisk (gzip) → load **phys `0x1000000`**, size `0xD466E`.
- DTB = **QCDT**; Passport tree = **`qcom,msm8974-oslo`**; tags `0x100`;
  cmdline `androidboot.hardware=qcom … selinux=permissive buildvariant=userdebug`.
- (`recovery.img` in the same folder carries a similar kernel + the same QCDT.)

Handoff = copy kernel/ramdisk/DTB to their phys addresses, set `/chosen/bootargs`,
`cpsid if`, flush caches, MMU off, `r0=0 r1=0 r2=DTB`, branch to `0x8000`.

------------------------------------------------------------------------------
## 3. ⛔ The one remaining blocker: physical-memory access + an identity page
The ARM kexec needs (a) to write the images to **physical** addresses and (b) an
**identity-mapped** page to run from once the MMU is off. Everything userspace is
closed:
- `mmap` / `mmap64` / `mmap_device_memory` → **`EINVAL`** for anon, phys, device
  (all flag/fd combos). The kernel reaches these via its **VM subsystem**
  (`vm/vm_physobj.c`, `vm_physobj_t`, `phys_range_t`, `as_stats.map_phys`, `/dev/mem`).
- `/proc/1/as` exposes **only the kernel's own segments** (`0xfe174000`–`0xfe218000`);
  every low/table VA (`0x01412000`, `0x00e1c000`, `0x01413000`, …) reads ESRVRFAULT.
- **No identity / direct map:** `VA = PA + 0xEDFFF000` holds only for the kernel's
  ELF segments, not for RAM generally.
- `kercalls.h` has **no MMAP call**; `__KER_RING0` (call 2) only `blx`es a function
  pointer for threads already flagged ring0 (not a mapping primitive).

------------------------------------------------------------------------------
## 3b. mmap is unusable from our tools (EINVAL for everything)
Even a plain **file** mapping (`mmap`/`mmap64` of `/proc/boot/libc.so.3`,
`PROT_READ|MAP_SHARED`) returns `EINVAL(22)` — so it is **not** a flag/`off_t`
issue; the `mmap`/`mmap64` symbol our hand-built tool binds to isn't the real VM
entry. (The SDK shows `mmap`↔`mmap64` are alias/inline pairs; the device libc is
stripped.) `mmap_device_memory` also EINVAL. Conclusion: **userspace cannot get a
physical mapping; we must do it from ring0 or via the kernel's VM directly.**

------------------------------------------------------------------------------
## 3c. ⭐ BLOCKER CLEARED — physical memory + identity map work (qcc tool)
The `EINVAL` was our hand-built (stub-libc) tool binding the wrong `mmap` symbol.
A **qcc-built** tool with `procmgr_ability(ADN_ROOT…, MEM_PHYS)` gets:
- `mmap(MAP_ANON)` ok (RWX anon blocked by W^X),
- **`mmap_device_memory(addr=phys, …, MAP_PHYS, phys)` → true identity map (VA==PA) with `PROT_EXEC`**,
- **exec from a phys-mapped page works** (`mov r0,#42; bx lr` → 42),
- phys read/write works.
⇒ kexec trampoline is feasible: identity-map a code page, `cpsid if` → flush →
`SCTLR.M=0` → branch to Linux phys entry. Layout caveat: don't clobber QNX's page
tables at phys `0x01412000`/`0x00e1c000` — load the relocatable kernel/ramdisk/DTB
at safe high physical addresses.

------------------------------------------------------------------------------
## 4. Plan to clear the blocker (in order)1. **Find the kernel's mmap/VM entry** (`vm_physobj`) and the exact `mmap(MAP_PHYS)`
   parameter layout the kernel accepts — our EINVAL is almost certainly a malformed
   call, since drivers map phys routinely. Then map the page-table page and/or RAM
   (with `MEM_PHYS`, which we can grant via `procmgr_ability(ADN_ROOT,…)`).
2. Or have the **ring0 payload build its own 1:1 map**: it has CP15 + kernel
   write; it needs a VA that aliases the page tables (walk TTBR via a VA we locate).
3. Then: copy images → flush → MMU off from the identity page → jump to `0x8000`.
4. Bring up Android; package as an **autoloader**.

**Recovery note:** every change so far is a *reversible data write* (one call-table
pointer, restored). A mis-jump just reboots; nothing persistent is modified, and the
autoloader (`2-RECOVER-BB10-root_v2.exe`) restores BB10.

------------------------------------------------------------------------------
## 5. Artifacts
- Device tools (`/accounts/devuser/`): `asrw`, `asdump`, `memphys`, `phase1full`
  (ring0 proof), `mmtest*`. Host sources in `Temp/opencode/bb/asrw/`.
- Dumps: `procnto.bin` (kernel ELF), `kern.bin`, `kdata.bin`, `code.dis`, `libc.dis`.
- Android: `boot.img` (kernel+ramdisk+QCDT), `rec_kernel.bin`, `boot_dtb.bin`.
- Notes: session57 (RIMBOOT verif), session58 (OS-type/ring0 leads),
  session59 (kexec plan + ring0), this (status).
