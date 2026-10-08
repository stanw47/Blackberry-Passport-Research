# session45 — BB10 blocks server-side primitives in the devuser shell (conclusive)

Date 2026-10-08. Passport retail. Follows sessions 43–44 (startup fix; resmgr
ability gate). This closes the question "can we prototype binder transport from
the devuser shell?" — **no.**

## Kernel-call probes (sources in `tools/passport-a11/`)

| test | result |
|---|---|
| `getpid()` | works (reads libc state — confirms `_init_libc` ran) |
| `SchedYield()` (`svc 0x51`, ip=0x5a) | works — kernel calls function |
| `ChannelCreate(0)` (`svc 0x51`, ip=0x23) | **process silently exits (RC=0, no fault message, no return value)** |
| `resmgr_attach(any path)` | EPERM (session44) |

`kc_test2.c` calls `ChannelCreate` then `_exit(42-or-errorbyte)`; the shell
reported **RC=0** — the process never reached `_exit`, so the kernel/security
layer terminated it inside `ChannelCreate`.

## Conclusion

BB10's security model enforces **server-side primitives** (create a channel,
attach a resource manager) for unprivileged processes — denial either returns
EPERM (`resmgr_attach`) or terminates the process (`ChannelCreate`). Client-side
syscalls, file IO, `devctl` to existing devices (with matching creds), threads,
and the A11 userspace all work.

Therefore **no binder transport can be prototyped from the devuser shell**:
- real driver `/dev/binder` — needs uid 1000 / binder-group credentials
  (every dcmd → EACCES as devuser, session43);
- own resmgr — `resmgr_attach` EPERM, any path (session44);
- private channel broker — `ChannelCreate` kills the process (this note).

All three succeed only in the **product launch context** (the Android uid +
abilities BB10 grants its Android runtime — the same context the retail player
runs in). This is consistent with the sole-OS plan: our runtime replaces/joins
that context.

## What to still build from the shell

Everything userspace: A11 chain, libs, probes, translation layers, host unit
tests. Binder transport integration testing is deferred to the product launch.

## Device state

`/dev/binder` 660; no probes running; A11 chain intact.
