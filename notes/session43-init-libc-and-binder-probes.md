# session43 — `_init_libc` startup fix + binder driver probe results

Date 2026-10-08. Passport (retail E538). Continues sessions 39–42. The fix
itself lands in the Classic repo's WS1 harness (`ws1/start.S`,
`ws1/gen_tramps.py`) — see Classic `notes/session78-init-libc-fix.md`.

## Discovery: our freestanding probes skipped QNX libc startup

`devctl()` from our probes SIGSEGV'd inside the "loader" at
`ldqnx.so.2@_connect_ctrl+0x3c` — even on `/dev/null`. Root causes:

- **`/usr/lib/ldqnx.so.2` is `libc.so.3`** — pulled it from the Passport:
  identical md5 `736f43e4…`. The interpreter *is* the C library, so the crash
  was inside libc's connection helper.
- Our `ws1/start.S` was literally `b main` — **`_init_libc` never ran**, so
  libc's connection/fd-side state was uninitialized. `open/write/close` don't
  need it; `devctl` does.
- Reference for the correct startup: RIM's `__android_system` entry
  (`@0x8e8`): `argc=[sp]`, `argv=sp+4`, `envp=&argv[argc+1]`, scan past envp,
  **`bl _init_libc`**, init arrays, `main`, `exit`.

## Fix (Classic repo, deployed shim)

- `ws1/start.S` now mirrors that crt sequence and calls
  `_init_libc(argc, argv, envp)` before `main`.
- `_init_libc` and `devctl` were added to the shim's exports (trampoline +
  `qnxb_ptrs` direct binding) so calls bind through the shim, not the loader.
- All probes rebuilt (`build-tb.sh`); copy in `tools/passport-a11/start.S`.

## Verified on the Passport

- `devctl("/dev/null", bogus)` → rc=25 (ENOTTY), no crash.
- `tb_shim` → RC=0; `tb_a11` → `UBS N=0x10a20770` RC=0 (new startup).

## Binder driver probes (with fixed startup)

- `probe_binder_step` (test-only `chmod 666 /dev/binder`; restored to 660 after):
  - open `/dev/binder` → fd=3 ✔
  - plain `ioctl(BINDER_VERSION)` → `errno=13` EACCES (driver wants
    `ioctl_binder`→devctl, as discovered in session40)
- `probe_binder_devctl` (RIM's dcmds via QNX devctl):
  - `0xC0046209` VERSION → EACCES
  - `0xC108620C` CFG → EACCES
  - `0xC03C620B` TXN → EACCES
  - `0xC0186201` WR → EACCES
  - all `rc=13` under devuser (uid 100) — consistent with a **credential
    check** (device is `1000:10011`; the retail runtime runs as the Android
    uid). No more loader crashes.

## Next

1. Drive the driver with the **Android credentials** (product path via BB10's
   launch framework; not reachable from the devuser shell — root/setuid
   wrappers are blocked by BB10 policy).
2. Port `ioctl_binder` + the 32↔64-bit wire translation (sessions 40–42,
   `tools/passport-a11/ioctl_binder.dis`).

Device state: `/dev/binder` restored to `660`; A11 chain intact; probes
stopped.
