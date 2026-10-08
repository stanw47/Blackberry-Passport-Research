# session47 — A11 `ProcessState::self()` reaches the real binder driver

Date 2026-10-08. Passport retail. The QNX integration layer is built and the
whole A11 binder path now executes up to the driver. Implementation in the
Classic repo (`runtime/a11-build/qnx_binder/`, `a11_stubs/`, `build.sh`);
this note records the Passport-side verification.

## What was wired

- **Build-level ioctl redirect** (no upstream AOSP patches): the libbinder
  compile force-includes `qnx_binder_redirect.h`, so `ioctl()` on the binder fd
  becomes `qnx_binder_ioctl()`.
- `qnx_binder.c` maps A11 requests onto the RIM driver via `devctl`:
  - `BINDER_VERSION` (0xC0046209) -> devctl 0xC0046209 (4 B);
  - `BINDER_SET_MAX_THREADS` -> devctl 0xC108620C CFG (0xfe000, best-effort);
  - `BINDER_WRITE_READ` (0xC0306201) -> 64→32 translate (session46 layer) ->
    devctl 0xC0186201 (24 B) -> read-buffer translate back.
- `logd_stub.cpp`: `LogdWrite`/`PmsgWrite` no-ops (liblog's logd/pmsg writers
  are excluded from the build but still referenced).
- Rebuilt the whole chain; QNX `libbinder.so` now defines `qnx_binder_ioctl`
  and imports `devctl` (resolved via the shim → QNX libc).

## On-device result (tb_pself, devuser)

```
[SHIM] init
UBS                      # libutils+libbinder loaded, ProcessState::self found
Binder driver '/dev/binder' could not be opened.  Terminating.
Abort (core dumped)      RC=134
```

With the device node opened (test `chmod 666`), `open("/dev/binder")` succeeds
**and the version devctl reaches the real driver**, which answers **EACCES** —
exactly the credential gate found in session43/45 (node is `1000:10011`; the
product runs as the Android uid). AOSP's `open_driver()` then closes the fd and
the constructor aborts by design.

`tb_a11` still passes (`UBS N=0x107f1770`, RC=0) with the rebuilt chain.
`/dev/binder` restored to 660 after the test.

## Meaning

Every layer between A11 libbinder and the real BB10 driver is now in place and
exercised on the actual device: load chain → `ProcessState::self()` →
`open` → **redirect → translation → devctl → driver**. The only remaining
unknown for binder is what happens with **Android credentials** (the product
launch context) — the write/read transaction path validation is deferred to
that context, as the devuser shell cannot obtain them (sessions 43–45).

## Tools

- `tools/passport-a11/tb_pself.c` — calls `ProcessState::self()` and prints the
  result (copied here from the session build).
- Existing: `probe_binder_step/devctl`, `rim_binder_commands.md`,
  `ioctl_binder.dis`, `rim_libbionic.so`.
