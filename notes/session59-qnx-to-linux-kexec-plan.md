# Session 59 — Implementation plan: Android on the retail Passport via a
# QNX→Linux "kexec" handoff (software-only, boot chain untouched)

## ✅ PHASE 1 DONE — RING0 CODE EXECUTION ACHIEVED (2026-10-13)- Kernel call dispatch found: `ldr pc,[r2,ip,lsl #2]` @ `0xfe1aab5c`, table base
  **`0xfe211378`** (RW data), 106 entries; call# in `ip`/r12.
- Userspace trigger convention (from the kernel's own stubs): **`mov r12,#N; svc #0x51`**.
- Proof: saved call **#105** (`__KER_POWER_SPARE4`, orig `0xfe1e057c`, slot
  `0xfe21151c`) → wrote the helper's `.text` payload VA → `svc #0x51` with r12=105
  → the kernel `blx`'d the **user page** and ran it at **ring0** (wrote
  `0x0BADF00D` to kernel cell `0xfe218000`) → **restored** `0xfe1e057c`. Device alive.
- ⇒ We have **arbitrary ring0 code execution** (ARMv7, no PXN/SMEP), reversible,
  boot chain untouched, no eMMC/WP.


Date 2026-10-13. Retail Passport (SQW100-1, MSM8974AA, BB10/QNX 6.5, Krait
ARMv7). Follows session58 (the `/proc/1/as` kernel-R/W breakthrough).

------------------------------------------------------------------------------
## 0. Goal & constraints
- End state: the device boots **balika LineageOS 18.1 (Android 11, Linux kernel)**
  with the **RIMBOOT/QNX boot chain left intact** (no boot0 write, no desolder,
  no CMD6/WP games — those brick).
- Delivery: an **installable** — autoloader (QCFM user-area) or a uid-0 script —
  for any rooted BB10 device.
- Two OSes must not run at once (battery/perf): QNX runs only long enough to hand
  control to Linux, then Linux/Android owns the machine.

------------------------------------------------------------------------------
## 1. The primitive we now hold (session58)
- **uid-0** via `pathtrust`/`__root`.
- **`/proc/1/as` read AND write** on procnto (the kernel):
  - code `0xfe173000`–`0xfe207ac8`  : **RO** (never touch)
  - data `0xfe208000`–`0xfe219870`  : **RW**  ← our only write target
  - confirmed `write(8)=8` at `0xfe208000`.
- **986 kernel function pointers** densely packed from `0xfe208000` (kernel
  call/dispatch table candidate); 411 data pointers. ELF: static, stripped,
  entry `0xfe1a6518`, machine ARM (Krait, ARMv7 — **no PAN, and v7 has PXN**).
- Tools: `asrw`/`asdump` (read/write/dump `/proc/<pid>/as`), `memphys`
  (`procmgr_ability`), all built with the fixed MSYS-clean ARM recipe.

**Rule for every write:** only the RW data range; save the original bytes first;
never write code; every step is reversible (data-only).

------------------------------------------------------------------------------
## 2. Why this can work (the core trick)
On **ARMv7** there is **no PAN/SMEP** (those are ARMv8.1). A whole CPU is either
supervisor (kernel) or user at a time; a *user* page is normally accessible from
supervisor mode unless **PXN** is set on it. So:

> Retarget a **kernel function pointer** (in our RW window) to a page **we mapped
> executable in our own process**. When the kernel later *calls* that pointer, it
> runs on our process's address space (kernel + our user mappings) in **supervisor
> mode** → our code executes at **ring0**.

This avoids writing kernel code entirely. If PXN blocks it (Phase 1b), we flip the
PTE's PXN bit (page tables are kernel *data* → writable), or fall back to a
kernel-code gadget.

------------------------------------------------------------------------------
## 3. Phases

### Phase 0 — Primitive + RE  [done]
- `/proc/1/as` R/W proven; `kern.bin`/`kdata.bin` dumped; call-table located.

### Phase 1 — Prove ring0 execution (least destructive)
0. **Payload placement (no `mmap` needed).** `mmap(MAP_ANON)` is rejected
   (`EINVAL`) on this build for every flag/fd combo, so don't rely on it. Instead
   **compile the ring0 payload as a function in the helper binary's own `.text`**
   — it is already RWX-as-RO/executable and *is mapped in the caller's address
   space*, which is the one the kernel uses while servicing our syscall. Print its
   runtime VA (or read it from `/proc/<pid>/mappings`).
1. Payload: *"store 0xDEADBEEF to a chosen kernel-data cell; return."*
2. Save the original pointer of a **rarely-used** call-table entry (e.g. a
   QNX-specific/obsolete kernel call we can invoke once); write our payload VA.
3. Invoke that kernel call → check the magic appeared in kernel data.
4. Restore the original pointer. (If it faults: PXN → go to 1b.)
**1b (fallback):** locate the PTE for our page in the kernel's page tables
(a data structure) and clear its PXN bit, then retry.

### Phase 2 — Self-contained ring0 kexec trampoline
**Targets extracted** (`F:\FLASH-AFTER-REPAIR\lineage-18.1-...wseries.zip` → `boot.img`):
- kernel `Image` (uncompressed, standard ARM head): file 0x800, **load phys 0x8000**,
  size **0x517F88**; first bytes `0000a0e1...` (NOP stub).
- ramdisk (gzip): **load phys 0x1000000**, size **0xD466E**.
- DTB = **QCDT** (1.2 MB) appended; the Passport's is the **`qcom,msm8974-oslo`**
  tree (oslo = the Passport Android codename): candidates at 0x99800 / 0xcc800 /
  0xff800 (select by SoC rev; retail model-id `0x87002C0A`).
- header `tags_addr=0x100`, `page_size=0x800`, cmdline
  `androidboot.hardware=qcom ... androidboot.selinux=permissive buildvariant=userdebug`.
- (recovery.img carries a similar kernel + the same QCDT, usable as a fallback.)

Prep (userspace, before the handoff):
- Lay out in RAM: the kernel `Image`, the ramdisk, and the selected **oslo DTB**;
  set the cmdline in the DTB `/chosen/bootargs`.
Ring0 payload (no return):
- `cpsid if` (mask IRQ/FIQ).
- Ensure the trampoline runs from **identity-mapped** memory (VA==PA) so it
  survives MMU-off (allocate a phys-contiguous identity region, or reuse QNX's
  identity window).
- Clean+invalidate D-cache / invalidate I-cache+TLB for kernel, DTB, trampoline.
- Disable MMU (`SCTLR.M=0`) and caches.
- `r0=0`, `r1=0` (DT boot), `r2=` DTB **physical** address.
- Branch to the Linux kernel entry (physical 0x8000).

### Phase 3 — Bring up Android
- Linux boots → balika ramdisk/`init` → mount the Android `system`/`vendor`/
  `data` from the writable user area (balika's layout). dm-verity off; SELinux
  permissive for bring-up.

### Phase 4 — Persistence / delivery ("sole-OS")
- Ship: an **autoloader** (QCFM user-area records: our loader + the Android
  images) — the updater already writes user-area records; **no boot0**.
- On boot: QNX starts (seconds), our startup hook in the writable RCFS triggers
  the ring0 handoff immediately → Linux/Android owns the machine. Minimal time
  spent in QNX (battery/perf ≈ solo Linux).

------------------------------------------------------------------------------
## 4. Risks & fallbacks
| risk | mitigation / fallback |
|---|---|
| PXN blocks user-page ring0 exec | flip the page's PXN in kernel PTEs (data write) |
| Kernel code RO; no W+X | we never need W+X — run from an existing user X page |
| Handoff crash | device reboots; data-only changes are reversible; autoloader recovers |
| DTB / machine-type wrong for Linux | derive from balika `boot.img` + QNX device tree dump |
| QNX hardware state ≠ Linux expectations | bring-up from balika (same SoC) |
| If kexec proves intractable | **route #2**: Android-on-QNX runtime (QNX kernel stays) |

------------------------------------------------------------------------------
## 5. Immediate next steps
1. **Phase 1**: build the RWX payload + call-table redirect; prove ring0 exec and
   determine PXN behaviour. (Only writes: one pointer, restored after.)
2. Pull **balika `boot.img`** (kernel + ramdisk) and its **DTB**; identify the
   MSM8974 machine-type / DT compatible.
3. Map the kernel's **page tables** in `kdata.bin` (for Phase 1b and for the
   identity-mapped trampoline region).

------------------------------------------------------------------------------
## 6. Artifacts
- `procnto.bin` (kernel ELF), `kern.bin` (live code), `kdata.bin` (live data),
  `asrw`/`asdump`/`memphys` sources + build recipe.
- balika: `F:\FLASH-AFTER-REPAIR\...` / `Passport_Android_balika_v2.exe` (F: drive).
