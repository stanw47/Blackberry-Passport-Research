# session40 — Binder: BB10 driver interface discovered (RIM `ioctl_binder`)

Date 2026-10-08. Passport (retail E538). The A11 chain now runs (session39);
the next milestone is real binder. Reference for everything here is the
**Passport 4.3 runtime dump** (this repo: `recon/passport-runtime-4.3/`, local
copy `~/priv-research/working/passport-player43/extracted/`). Related Classic
repo material: `binder/notes-session77-driver-interface.md` (same findings, kept
there because the A11 binder port lives in the Classic repo), `binder/BUILD.md`,
`binder/notes-session63/64/65*` (the separately-developed binder resmgr, parked:
`resmgr_attach('/dev/binder')` → EPERM on BB10).

## The device's binder

- `/dev/binder` exists: `nrw-rw---- 1000 10011` (uid 1000 = android system,
  gid 10011; no named group).
- RIM's 4.3 runtime uses it (`libbinder.so` strings: "Opening '/dev/binder'
  failed", "Using /dev/binder failed: unable to get transaction memory").
- **RIM's libbinder never calls plain `ioctl()`** — it imports
  **`ioctl_binder`**, which is defined in RIM's `libbionic.so`
  (`T ioctl_binder @0xf228`; imports `devctl`, `MsgSendnc`, `MsgSendv_r`,
  `MsgSendvsnc`). It translates the binder ioctls into QNX devctl/MsgSend
  messages.

## Request numbers RIM actually uses (read from libbinder disassembly)

| call site | request | arg |
|---|---|---|
| `ProcessState::ProcessState()` @0x27f70 / `C1Ev` @0x2808a | **0xC108620C** | struct with size field at +0x104 = **0xfe000** (1 MiB − 8 KiB) |
| `IPCThreadState::talkWithDriver()` @0x22c10 | **0xC0186201** | `struct binder_write_read` **24 B** (32-bit layout) |
| version path @0x2799e | **0xC0046209** | 4 B (`_IOWR('b',9,int)`) |
| `binder_qnx_fd()` @0x287bc (T @0x28780) | **0xC03C620B** | 60 B — "transaction memory" |

`ioctl_binder` itself handles at least `0xC0186201` and `0xC108620C`
(compared in its first block; builds messages with a 16-bit type field `0x106`).

## Probe results on the Passport

- `open("/dev/binder", O_RDWR)` as devuser → EACCES (device 1000:10011).
  Test-only `chmod 666` (restored to 660 after) → **fd=3 OK**.
- Plain `ioctl(fd, BINDER_VERSION /*0xC0046209*/)` → rc=-1 **EACCES** — the
  driver rejects QNX libc's plain ioctl path; RIM goes through `ioctl_binder`.
- `devctl(fd, …)` probe → **SIGSEGV inside `ldqnx.so.2`**
  (`_connect_ctrl+0x3c`, ref=fd) even with `LD_BIND_NOW=1` — not yet root-caused.
- Running as uid 1000 is not reachable from a shell: BB10 blocks root from
  exec'ing setuid binaries (`__android_system`, 4488 B, uses `system()` +
  `procmgr_ability` — meant to be run by the OS framework). The product
  runtime will run as the Android uid via BB10's launch framework anyway.

## Wire-format catch: 32-bit (RIM 4.3) vs 64-bit (A11)

- RIM 4.3 driver: `BINDER_WRITE_READ = 0xC0186201` → **24-byte**
  `binder_write_read` (32-bit pointers) — the pre-Android-8 binder wire format.
- A11 libbinder (AOSP android-11): `BINDER_WRITE_READ = 0xC0306201` →
  **48-byte** structure (64-bit binder ABI; Android removed 32-bit binder
  support and 32-bit apps use the 64-bit ABI).
- So even with `ioctl_binder` ported, the A11 libbinder ↔ BB10 driver path
  needs a **32↔64-bit wire translation** (that is precisely what our parked
  binder resmgr was built to avoid by serving the 64-bit ABI itself).

## Status of the parked resmgr route

`binder/` (Classic repo): full userspace binder engine (host-validated, 24 ABI
+ 23 engine + 6 glue tests green), runs on device, but
`resmgr_attach('/dev/binder')` → EPERM (BB10 ability model blocks non-system
processes from attaching that path; same class as root exec restrictions).

## Next options

1. Port `ioctl_binder` (disassemble fully: `0xf228–0xf424` in RIM libbionic.so)
   and build a 32↔64-bit translation layer in front of the real driver; or
2. Get the resmgr route past EPERM (would need a system-signed process / different
   path), or
3. Provide a userspace binder **library-level** shim: intercept A11 libbinder's
   driver calls (patch the qnx-linked A11 libbinder to use an internal
   transport) — i.e., bypass `/dev/binder` entirely for our port.

Tools: `tools/passport-a11/probe_binder_step.c`, `probe_binder_devctl.c`.
Device state after this session: `/dev/binder` 660 restored; probes stopped.
