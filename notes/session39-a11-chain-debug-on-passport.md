# session39 — A11 native chain debug on the Passport: pre-ctor crash → running

Date 2026-10-08. All work on the retail Passport (E538, rooted, dev mode).
Build harness/shim details live in the **Classic repo** — this note records the
Passport-side work and cross-references it:

- Classic repo `notes/session73-a11-precto-crash.md` — crash localization
- Classic repo `notes/session74-prector-dlopen-crash.md` — shim gap fixes,
  pre-ctor dlopen crash, pdebug blocked by BB10 /proc policy
- Classic repo `notes/session75-linktime-binding-hang.md` — link-time binding
  architecture, slot-stride bug, pdebug tooling notes
- Classic repo `notes/session76-passport-a11-chain-runs.md` — root cause +
  milestone (published there before this repo was brought up to date)

## Symptom

`tb_shim` (WS1 shim only) ran; `tb_cxx`/`tb_a11` (libc++.so in NEEDED) **hung
silently pre-init** (thread state READY, tight user-space spin) or earlier
crashed pre-ctor at `libc.so.3+0x49f7c` / `ws1_resolver`.

## What was ruled out (on the Passport)

- Deployed chain == local builds (md5s) — not stale.
- Passport vs Classic `libc.so.3`: same 598,616 B file, **4 differing bytes**,
  all in ELF program-header `p_paddr` fields — libc code identical.
- `LD_PRELOAD` no effect; `LD_NOINIT` did not suppress shim ctors (unreliable).

## Fixes landed in the chain during this work (details in Classic notes)

1. `__brk` shim bug removed (QNX has no `__brk`; use dlsym'd `brk`).
2. `__emutls_get_address` implemented (clang emutls ABI, pthread-key based) and
   `dl_unwind_find_exidx` stub — `LD_BIND_NOW` now resolves all symbols.
3. Link-time alias binding (`libqnxbind.so`, `qnxb_ptrs[]` table) — the shim
   resolver no longer calls dlopen/dlsym at resolution time.
4. `ws1_resolver.S` slot stride fixed 12 → 16 bytes (a stale stride read garbage
   slots and produced the silent spin).
5. Shim `__errno` now forwards to QNX `__get_errno_ptr` (real per-thread errno).

## ROOT CAUSE of the pre-init spin: NEEDED link order

Bisected with two trivial write-only probes built for the Passport:

- NEEDED `[libc.so, libc.so.3]` → **hangs before any ctor output**
- NEEDED `[libc.so.3, libc.so]` → runs

The shim (`libc.so`) must not be loaded before the real `libc.so.3`. The A11
probes linked `libc++.so` first, which pulled `libc.so` into the link map ahead
of `libc.so.3`. Fixed link order (`-l:libc.so.3 -l:libc++.so -l:libc.so`).

## Milestone on the Passport (retail E538)

    tb_shim3n -> [SHIM] init / A11 libs loaded / RC=0
    tb_cxx    -> [SHIM] init / A11 libs loaded / RC=0
    tb_a11    -> [SHIM] init
                 tb_a11: sigaction rc=0x0
                 UBS N=0x106e4770    # libutils+libbinder dlopen OK,
                                     # ProcessState::self found, libc++
                                     # operator new returns heap
                 RC=0

`LD_BIND_NOW=1` shows `ubs` (libutils eager-bind only) — lazy is the default;
TODO only if ever needed.

## Tooling used on the Passport (this arc)

- `LD_DEBUG=libs|all|bindings` on the device — the key diagnostic.
- `tb_wonly`/`tb_wonly2` (write-only probes) — NEEDED-order bisection.
- `tb_qnxb` — prints `qnxb_ptrs` values from a working context.
- `/usr/bin/pdebug` (QNX GDB agent): attached but **BB10 blocks attach**
  (devctl 0x40e00805 on `/proc/<pid>/as` fails even as root). Protocol decoded
  from the unstripped x86 pdebug in the SDP
  (`TargetRegrd`: `[11][regset][counter u16][offset u16][size u16]`).
- `slay -f -s 9 <name|pid>` to stop spinning probes (they saturate the device
  at prio 10r).
- Root cannot exec probes from the shared folder or /tmp ("Operation not
  permitted" — BB10 ability policy), so root-side debugging stays limited.

## Where things are

- Probe sources (copy): `tools/passport-a11/` in this repo.
- Canonical harness: Classic repo `runtime/a11-build/test/build-tb.sh`,
  `ws1/` shim, `sysroot/target` (libc.so.3 stub), `binder/` port sources.
- Deployed chain on device: `/accounts/1000/shared/misc/android/qnx/`
  (`libc.so`, `libqnxbind.so`, `libc++.so`, `libutils.so`, `libbinder.so`,
  `libbase.so`, `liblog.so`, `libcutils.so`, probes).

## Next

Binder for real (see session40): the A11 libbinder must talk to the BB10
`/dev/binder` driver — driver interface discovered in RIM's libbionic
(`ioctl_binder`), plus the 32-bit vs 64-bit wire-format question.
