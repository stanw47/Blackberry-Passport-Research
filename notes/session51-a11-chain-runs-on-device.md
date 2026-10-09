# session51 — A11 binder chain runs in the player context; first transaction reaches the driver

Date 2026-10-09. Passport retail. Continuation of session50. Huge session:
the complete deployment architecture, the driver's txn-memory mechanism, and
the A11 chain running end-to-end on-device. One blocker remains (driver EINVAL
on the PING transaction).

## Headline results

`tb_pself` (A11 chain, player context, real device):

```
[SHIM] init trace50f
UBS
QB ioctlc0046209        VERSION -> 7
QB ioctl40046205        SET_MAX_THREADS (mapped to RIM 0x80046205)
QB mmap fd00000003      mmap intercept
QB token28fbf000        CFG -> peer-mapped txn memory
 self=0x10479050        ProcessState::self() COMPLETE
D
... TLS: KC/KG/KS (key_create/getspecific/setspecific) all work
QB ioctlc0186201        BINDER_WRITE_READ — first real transaction
QB wr rc00000016        driver replies EINVAL  <-- last blocker
 dsm=0x10475260         defaultServiceManager() returns
probe RC=0              clean exit
```

So: open_driver, version check, SET_MAX_THREADS, txn-memory setup, Parcel,
IPCThreadState/TLS, and the WRITE_READ syscall path all work in the player
context with the A11 libbinder built 32-bit-wire. The only failing piece is
the driver's validation of the transaction contents.

## The deployment architecture (works, keep it)

The runtime's own QNX binaries (`lowmemorykiller`, `logd`, `android_resmgr`,
`epolld` NEED `libcutils`; `servicemanager`/`app_process`/`libandroid_runtime`
NEED `liblog`/`libutils`/`libbinder`) **must keep the real system/lib libs**,
or the core start aborts (set -e) before `binder` starts.  Our A11 chain lives
in **unused trusted inodes** and is exposed to the probe via a search-path
directory:

- chain inodes (content replaced in place, trust preserved):
  `libc++.so <- libvideoeditorplayer`, `libbase.so <- libttscompat`,
  `libcutils.so <- libvideoeditor_jni`, `liblog.so <- libSR_AudioIn`,
  `libutils.so <- libnfc_ndef`, `libbinder.so <- libbcc.sha1`
  (all verified unused: only self-soname references),
- shim: `native/lib/libthread_db.so <- ws1 libc.so rebuilt with SONAME
  libthread_db.so` (Makefile target `build/libthread_db.so`), chain NEEDs
  `[libc.so.3, libthread_db.so]` (order required, session57),
- `native/lib/libqnxbind.so <- libstdc++.so` inode (shim's NEEDED),
- `native/lib/libc.so` stays REAL (core start),
- `$NS/ourprobe/lib/` = **hard links** (symlinks are not followed by the QNX
  loader reliably) to all of the above, with the natural names,
- wrapper11 runs the probe with `LD_LIBRARY_PATH=$NS/ourprobe/lib:$LD_LIBRARY_PATH`
  (only the probe; the core's own script sets its own path),
- `dexdump` (trusted inode) = the probe binary.

`Resource busy` when overwriting = file is mapped; `slay -f -s 9 dexdump` first.

## The driver's txn-memory mechanism (cracked)

From `system/bin/binder` (has symbols!): `binder_devctl` @0x4128,
`binder_thread_write` @0x2a0c.

- **CFG (0xC108620C, 0x108 B, vm_size at +0x104)** — the ctor config: the
  driver does `getpid()`, creates `/dev/shmem/binder_<pid>`, maps it, and
  **peer-maps it into the CLIENT**; the client-side address is written back at
  **cfg+0x100**.  That address is the txn memory (mVMStart).  Verified: the
  probe read AND wrote at the writeback address.  `mmap()` on the driver fd
  is NOT supported.
- **TXN (0xC03C620B, 60 B)** — rejects everything we tried (fd/size/addr in
  blob[0]); not needed by the client path.
- **SET_MAX_THREADS**: only RIM's word 0x80046205 works (A11's 0x40046205 =
  ENOSYS).
- **WRITE_READ** bounds-checks the bwr buffers against the txn region; both
  region-relative offsets and absolute addresses pass the bounds checks.
- Command dispatch in `binder_thread_write` is full-word but accepts
  **BC_TRANSACTION 0x80286300 / BC_REPLY 0x80286301** via
  `r6 + 0x7fd79d00 <= 1` (0x2b1c -> txn handler 0x2f26).  A11's direction bits
  must be swapped (`rim = (c&0x3FFFFFFF) | ((c&0x40000000)<<1) | ((c&0x80000000)>>1)`).
- The txn struct is 40 B; the driver reads flags at +0xc (both layouts agree);
  RIM's `writeTransactionData` builds `buffer, offsets_size, offsets,
  data_size` at +0x18..+0x24 (AOSP order is `data_size, offsets_size, buffer,
  offsets`) — both layouts were tried.

## The shim fixes that were required (all in ws1/ and a11-build/)

1. **pthread redirect** (`qnx_bionic_redirect.h`, force-included in every
   chain lib): libc.so.3 is FIRST in NEEDED and exports pthread_mutex_*/
   cond_*/once/key_*, shadowing the shim's bionic 4-byte implementations ->
   the chain must call `ws1_impl_pthread_*` by macro.  Without this,
   IPCThreadState hangs.
2. **getrlimit**: QNX writes 16 bytes (64-bit rlim_t) into bionic's 8-byte
   struct rlimit -> smashed the caller's stack frame (SIGBUS in
   Parcel::Parcel's epilogue).  Shim now returns a fixed limit without QNX.
3. **pthread_once**: bionic 4-byte word implemented over the futex emulation.
4. **LOG_NDEBUG=1** for the chain build (debug logs off).
5. `qnx_binder.c`: devctl redirect + CFG-on-mmap + txn-memory writeback +
   WRITE_READ translation (offset/pointer + command-word swap + txn layout).
   **Currently contains a 14-variant auto-probe** (tries conventions 0..13 on
   EINVAL) — remove/trim once the EINVAL is understood.
6. `-DBINDER_IPC_32BIT=1` for libbinder (wire = RIM's 32-bit ABI).
7. Build gotcha: `set -e` in build.sh aborted after a qnx_binder.c compile
   error, silently skipping the AIDL objects -> chain failed to load.  Always
   check the build tail and run qnx-link.sh (it re-links; build.sh alone
   leaves the old .so).

## Remaining blocker (next session)

The driver replies **EINVAL** to the PING transaction under all 14 tested
conventions (buffers offsets/addresses, payload offsets/addresses, txn layout
AOSP/RIM, with/without synthesized payload, no-read, refcount-prefix).  The
command word IS accepted.  EINVAL sources in the txn path (driver strings):
"got transaction with invalid size %d-%d", "...invalid data ptr",
"...invalid offsets ptr", "...to invalid handle", plus "unknown command %d"
(the latter NOT ours — our word takes the 0x2b1c path).

Next steps:
1. Read the txn handler (0x2f26..0x3160) fully; find the "invalid ..." checks
   (the string refs use a logging helper — literal scans failed; use the
   logging helper @0x22ca call sites with their line numbers).
2. Try: flags=0 (TF_ACCEPT_FDS=0x10 may be rejected), sender_pid/euid filled,
   target handle != 0 (e.g. acquire a real handle first), data_size>=4 with
   the payload INSIDE the region as an absolute address while the bwr stays
   heap-like (mimic RIM exactly: heap bwr buffers).
3. Read RIM's txn handler requirements from their client side: RIM's
   `IPCThreadState::writeTransactionData` (0x22cee) shows their exact struct;
   RIM's Parcel allocates from **malloc** (heap pointers!) — so the bwr
   buffers are absolute heap addresses in RIM's client; try exactly that
   (heap buffer + absolute payload in the region).
4. slog2: driver logs go to /dev/slog2 (ring buffers, `cat` gives 0 bytes) —
   write a small slog2 reader (slog2info protocol) or use the driver's
   logging helper to learn the exact rejection.

## Process notes

- The automated restart trigger (LMK shrink + navigator invoke) works only
  while the runtime is up; once the core is down only the user opening an app
  revives it.  Many cycles were lost to stale logs — always check `date` and
  the log mtime before interpreting.
- USB tunnel dies on gadget resets; full reconnect ritual each time
  (fresh 4096-bit key; sometimes two auth passes).

## Commits

- Passport: this note.
- Classic: a11-build/{build.sh,qnx-link.sh,qnx_binder/*}, ws1/{Makefile,
  glue_impl.c,glue_core.c}, test/probe_binder_devctl.c.
