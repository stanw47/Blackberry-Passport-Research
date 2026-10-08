# session42 — `ioctl_binder` decoded: the BB10 binder driver call path

Date 2026-10-08. Passport (retail E538). Direct continuation of sessions 40–41.
Source of truth: RIM's own **`libbionic.so`** from the Passport 4.3 runtime dump
(`recon/passport-runtime-4.3/`, local copy under
`~/priv-research/working/passport-player43/extracted/.../native/system/lib/`).
The A11 binder port itself lives in the Classic repo (`binder/`); mirror note
there: `binder/notes-session77-driver-interface.md`.

## What `ioctl_binder` is

`T ioctl_binder @0xf228` in RIM libbionic.so (~0x1FC bytes of code). It is the
only bridge the retail Android runtime uses to the BB10 binder driver. It does
**not** call `ioctl(2)`; it builds QNX message structures and issues them with
`devctl`/`MsgSend*`. That is why a plain `ioctl()` to `/dev/binder` returns
EACCES while RIM's stack works.

## Request numbers seen at RIM's own call sites

| request | where it is used in RIM libbinder.so | notes |
|---|---|---|
| `0xC0186201` | `IPCThreadState::talkWithDriver()` | `BINDER_WRITE_READ`, 24-byte 32-bit layout |
| `0xC0046209` | version path | `BINDER_VERSION`, 4-byte arg |
| `0xC108620C` | `ProcessState::ProcessState()` ctors (`C1Ev`/`C2Ev`) | custom setup; 8-byte staging; struct field at +0x104 = `0xfe000` |
| `0xC03C620B` | `binder_qnx_fd()` (`T @0x28780`) | 60-byte arg; feeds the "transaction memory" mmap |

`ioctl_binder` compares the request against at least `0xC0186201` and
`0xC108620C`; all of the above are funnelled through it.

## ProcessState construction sequence (RIM, verbatim shape)

1. `open("/dev/binder", O_RDWR|O_CLOEXEC)` — fd stored at `ProcessState+4`.
2. `ioctl_binder(fd, 0xC108620C, &cfg)` — cfg buffer; field at `+0x104` is set
   to `0xfe000` (1040384 = 1 MiB − 8 KiB). This is done inside the ctor before
   threads/mmaps.
3. `binder_qnx_fd()` — reopens/dups the device and calls
   `ioctl_binder(fd, 0xC03C620B, &buf60, …)` to obtain transaction memory info,
   then the ctor mmaps it (the "unable to get transaction memory" error string
   lives next to this helper).
4. `ioctl_binder(fd, 0xC0046209, &version)` on the version-check path.
5. Transactions: `IPCThreadState::talkWithDriver()` repeatedly calls
   `ioctl_binder(fd, 0xC0186201, &bwr24)`.

## Why our A11 port cannot just call the driver yet

- A11 libbinder is built for the **64-bit binder ABI** (`BINDER_WRITE_READ =
  0xC0306201`, 48-byte `binder_write_read`; 64-bit `flat_binder_object`).
- RIM's driver + `ioctl_binder` speak the **32-bit 4.3 layout** (24-byte bwr).
- So the path is: A11 libbinder → (new) `ioctl_binder`-compatible bridge →
  **32↔64-bit wire translation** → driver. The alternative (private transport /
  our own resmgr) remains the fallback and is already implemented in the
  Classic `binder/` tree (blocked only by `resmgr_attach('/dev/binder')` EPERM).

## Practical bring-up constraints (measured on the Passport)

- `/dev/binder` = `nrw-rw---- 1000 10011`; devuser gets EACCES on open.
  Test-only `chmod 666` works and is restored to `660` after experiments.
- Root cannot exec our probes or setuid wrappers ("Operation not permitted" —
  BB10 ability policy). The product runtime will run under the Android uid via
  BB10's launch framework; that is the real credential path.
- `devctl(fd, …)` from our probe crashed inside `ldqnx.so.2`
  (`_connect_ctrl+0x3c`) even with `LD_BIND_NOW=1` — the bridge must not call
  devctl naively; port `ioctl_binder`'s exact message construction instead.

## Next actions

1. Fully annotate `ioctl_binder` (all branches): dump it in one piece
   (`objdump -d --start-address=0xf228 --stop-address=0xf424`) and write the
   exact staging structs for `0xC0186201`, `0xC0046209`, `0xC108620C`,
   `0xC03C620B` into the Classic `binder/` docs.
2. Implement the bridge (in the WS1 shim or a new `libioctlbinder.so`) and patch
   the qnx-linked A11 libbinder to call it.
3. Add the 32↔64-bit wire translation (bwr + flat_binder_object + node refs),
   then re-run `probe_binder_step`/`probe_binder_devctl` and a real
   `ProcessState::self()` + `open_driver()` against `/dev/binder`.

Tools: `tools/passport-a11/probe_binder_step.c`,
`tools/passport-a11/probe_binder_devctl.c`, `tools/passport-a11/binder_a11.h`.

Device state after this session: `/dev/binder` restored to `660`; no probes
running; A11 chain intact (`tb_a11` → `UBS`, RC=0).
