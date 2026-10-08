# session41 — Binder plan after the driver-interface discovery

Date 2026-10-08. Passport (retail E538). Follow-up to session40 (driver
interface in RIM's `libbionic.so`). This note is the Passport-side plan; the
A11 binder port + qnxinc headers live in the Classic repo (`binder/`), and the
same findings are recorded there as `binder/notes-session77-driver-interface.md`.

## Where we are

- A11 chain runs on the Passport: `tb_a11` → `UBS N=0x…`, RC=0 (session39).
- The real `/dev/binder` driver is RIM's custom QNX resmgr, driven through
  **`ioctl_binder`** (RIM libbionic.so). Verified requests:
  `0xC0186201` (BINDER_WRITE_READ, **24-byte** 32-bit layout),
  `0xC0046209` (VERSION, 4 B), `0xC108620C` (ProcessState ctor setup, size
  field `0xfe000`), `0xC03C620B` (binder_qnx_fd transaction memory, 60 B).
- Plain `ioctl()` to the driver → EACCES; `devctl()` probe → SIGSEGV in
  `ldqnx.so.2@_connect_ctrl+0x3c`. Device perms `1000:10011`; product will run
  as the Android uid via BB10's launch framework.
- Our parked binder resmgr (Classic `binder/`) runs but
  `resmgr_attach('/dev/binder')` → EPERM under BB10's ability model.
- Wire format: RIM 4.3 uses the pre-Android-8 **32-bit binder wire format**
  (24-byte write_read), while A11 libbinder uses the **64-bit ABI**
  (`0xC0306201`, 48-byte) even from 32-bit processes.

## Plan (choose by effort/robustness)

1. **Port `ioctl_binder` (preferred first step).**
   Source of truth: RIM `libbionic.so` `ioctl_binder @0xf228–0xf424` (in the
   session37 runtime dump / `recon/passport-runtime-4.3/`). Reimplement its
   message construction in the WS1 shim or as a small `libioctlbinder.so`, and
   patch the qnx-linked A11 libbinder (`ProcessState::open_driver()`,
   `IPCThreadState::talkWithDriver()`, plus `binder_qnx_fd()`'s
   transaction-memory flow) to call it.
2. **32↔64-bit wire translation.** Once the driver path works, A11 libbinder's
   48-byte `binder_write_read` (and 64-bit flat_binder_object etc.) must be
   translated to/from the driver's 32-bit layout. Scope it after step 1 by
   diffing the two UAPI headers and the driver's ioctl_binder field copying.
3. **Fallback: library-level binder transport.** If the real driver can't be
   driven from a non-system process, patch the A11 port's libbinder to use our
   own transport (the parked resmgr engine already implements the 64-bit ABI;
   it only needs to get past `/dev/binder` attach — e.g. a private path plus a
   small client shim inside libbinder).
4. **Credentials.** The product path runs as the Android uid (like the retail
   player); for bring-up, document the test-only `/dev/binder` chmod and always
   restore `660` afterwards.

## Immediate next actions

- Disassemble `ioctl_binder` fully (both variants if the code path branches per
  request) and write the translated request/arg layouts into
  `tools/passport-a11/` notes or the Classic `binder/` docs.
- Extract the driver's expected struct layouts from `binder_qnx_fd()` (60-byte
  arg) and the ProcessState ctor arg (0x104 offset field = 0xfe000).
- Re-test with the working probe once `ioctl_binder` is available (and, if
  reachable, as the Android uid via the launch framework rather than a shell).

Device state: `/dev/binder` 660 restored; no probes running.
