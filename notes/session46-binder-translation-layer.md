# session46 — binder wire-translation layer built + host-tested

Date 2026-10-08. Passport research. Implementation lives in the **Classic repo**
(`runtime/a11-build/qnx_binder/`); this note tracks the Passport-side result
and cross-references.

## What landed

A host-testable translation layer between the **A11 64-bit binder ABI** (our
libbinder) and the **BB10/RIM 4.3 32-bit ABI** (the real `/dev/binder` driver):

- `binder_compat.{h,c}` — command translation (size remap + **swapped ioctl
  direction bits**, verified against RIM's extracted values), `binder_write_read`
  both ways, and read/write **buffer walks** for the common BC_/BR_ commands:
  - `BC_TRANSACTION`/`BC_REPLY` (64B -> 40B txn), flat objects 24B -> 16B,
    offsets arrays u64 -> u32;
  - `*_DONE` ptr-cookie 16 -> 8, `FREE_BUFFER`/`DEAD_BINDER_DONE` ptr 8 -> 4,
    death-notification handle-cookie 12 -> 8;
  - read side: `BR_TRANSACTION`/`BR_REPLY` + `BR_INCREFS/ACQUIRE/RELEASE/
    DECREFS` + dead-binder pointers, back to 64-bit.
- `test_compat.c` — host unit tests. **All pass**, including the 12 command-word
  assertions against RIM's real values (`BC_TRANSACTION` -> `0x80286300`,
  `BR_TRANSACTION` -> `0x40287202`, etc.).

## Why this matters for the Passport

It pins down the one non-obvious risk in reusing the real driver with A11
libbinder (session40/42): RIM's command words differ (direction bits swapped +
32-bit struct sizes), and the transaction payload needs object/offset walking.
That is now specified and unit-tested off-device.

## Still to do

1. QNX-side client layer: `devctl(fd, {0xC0046209, 0xC108620C, 0xC03C620B,
   0xC0186201})` + the translators, patched into the qnx-linked A11 libbinder
   (`ProcessState::open_driver`, `IPCThreadState::talkWithDriver`).
2. Read-path blob expansion (32 -> 64) with an arena; `BINDER_TYPE_FDA/PTR`
   handling.
3. On-device validation **in the product launch context** — the devuser shell
   cannot do binder at all (sessions 43–45: EACCES on driver dcmds, EPERM on
   `resmgr_attach`, `ChannelCreate` kills the process).

## References

- Command table + rationale: `tools/passport-a11/rim_binder_commands.md`.
- Bridge disassembly: `tools/passport-a11/ioctl_binder.dis`, `rim_libbionic.so`.
- Driver request numbers: `notes/session40`, `notes/session42`.
