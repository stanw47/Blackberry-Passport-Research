# session44 — binder strategy after the resmgr ability-gate finding

Date 2026-10-08. Passport retail. Continues sessions 40–43. Classic-repo
details: `binder/notes-session79-resmgr-ability-gate.md`.

## New finding: user-space resource managers are blocked on BB10

We fixed the resmgr startup (`_init_libc`, session78/79) and linked the WS1
shim (for `__errno`), then ran the parked binder resmgr on the Passport:

```
[binder] init: before resmgr_attach(<path>)
resmgr_attach('<path>') failed: Operation not permitted
```

- `/accounts/1000/shared/misc/android/qnx/binder` → EPERM
- `/tmp/binder` → EPERM

So the failure is **not** about `/dev` — `resmgr_attach` is gated by a BB10
process ability that devuser processes do not have. The "own resmgr" binder
route is dead on BB10 for non-system processes.

## What remains

1. **Real driver in the product context (primary).** The retail runtime talks
   to `/dev/binder` as uid 1000 with Android abilities; our A11 runtime will be
   launched in that same context, where the driver accepts
   `ioctl_binder`/devctl traffic. From the devuser shell we cannot reproduce
   those credentials (root can't exec user files; setuid wrappers are blocked
   by BB10 policy), so on-device driver testing is deferred to the product
   launch path.
2. **Non-resmgr transport (fallback).** Patch the A11 libbinder transport to
   avoid `/dev/binder` and `resmgr_attach` entirely (in-process broker / QNX
   channels + side-band connection fd), or co-locate early services in one
   process. This keeps binder usable even outside the Android-credential
   context, at the cost of diverging from the AOSP transport.

## Practical rule

All Passport probes/daemons we build from the shell can: use libc syscalls,
devctl to *existing* devices (with matching credentials), open/write/mmap,
threads. They cannot: attach resource managers, open `1000:10011` devices as
devuser, or run as the Android uid. Keep this boundary in mind for future
bring-up steps.

## Immediate next work (unchanged primary plan)

Port the driver path for the product: `ioctl_binder` semantics are equivalent
to QNX `devctl(fd, dcmd, buf, nbytes, &info)` with RIM's dcmds
(`0xC0046209` VERSION, `0xC108620C` CFG, `0xC03C620B` TXN, `0xC0186201` WR —
session40/42), so the port mainly needs:
1. patching the qnx-linked A11 libbinder's `ioctl()` calls to `devctl()` with
   RIM's dcmds, and
2. the 32-bit (RIM 4.3 wire) ↔ 64-bit (A11 wire) translation layer for
   `binder_write_read`/`flat_binder_object`/transaction data.

Tools/artifacts: `tools/passport-a11/` (probes, `ioctl_binder.dis`,
`rim_libbionic.so`, `start.S`). Device state: `/dev/binder` 660; resmgr not
running; A11 chain intact.
