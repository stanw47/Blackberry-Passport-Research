# session52 — binder txn mechanism fully decoded (message offsets); current state

Date 2026-10-09 (evening continuation of session51). Passport retail.

## THE KEY DISCOVERY: everything is a devctl-message offset

The driver reads client data with **`resmgr_msgread(ctp, dst, size, offset)`**
(PLT slot verified in `system/bin/binder`).  Therefore:

- **The WRITE_READ devctl message is one big buffer**:
  `[binder_write_read 24 B][write stream + txn payloads][read area]`
- `bwr.write_buffer` and `bwr.read_buffer` are **offsets into that message**
  (write_buffer = 24 = right after the bwr; read_buffer = after the payloads).
- The txn's `data.ptr.buffer` / `data.ptr.offsets` are also **message offsets**;
  the driver copies them out with `resmgr_msgread` and the "invalid data ptr" /
  "invalid offsets ptr" checks are exactly `msgread(...) != data_size/offsets_size`
  (code at 0x34c4 / 0x34f8, log refs at 0x34de / 0x3512).
- The earlier bwr bounds check (`write_buffer + write_size <= [sl+0x50]`) is
  against the message length — which is why *every* offset/address variant
  failed while we only sent a 24-byte devctl: any offset > 24 was rejected.

Our shim now (uncommitted at time of writing, then committed):
`runtime/a11-build/qnx_binder/qnx_binder.c` — `pack_writemsg()` builds the
single message, swaps BC words to RIM's encoding, copies payloads into the
message and rewrites their pointers to message offsets;
`unpack_readmsg()` turns the reply's message offsets back into absolute
pointers into the caller's read buffer and swaps BR words back to A11.

## Transaction layout VERIFIED = AOSP (no reorder!)

RIM's `IPCThreadState::writeTransactionData` (0x22cee) calls
`Parcel::ipcDataSize()` -> +0x18, `ipcObjectsCount()*4` -> +0x1c,
`ipcData()` -> +0x20, `ipcObjects()` -> +0x24.  So the wire struct is
`target, cookie, code, flags, pid, euid, data_size, offsets_size, buffer,
offsets` — **the AOSP order** (an earlier "reordered" reading was wrong; the
`binder_transaction_log_entry` fill in the driver confirms +0x18=data_size,
+0x1c=offsets_size).

RIM's `talkWithDriver` (0x22b74) sets `bwr.write_buffer = mOut.ipcData()`
(= raw `mData`, a **heap pointer** in RIM's client) etc. — but the driver
treats those as message offsets, so RIM's client must be sending the data
inline in the devctl message (the `ipcData()` pointer is only used to build
the message).

## PING payload

A11's `getStrongProxyForHandle(0)` PING sends `data_size = 0`; RIM's 4.3
client always wrote `writeInt32(0)` (4 bytes).  The shim now synthesizes a
4-byte zero payload for `BC_TRANSACTION` when `data_size == 0` (in
`pack_writemsg`), matching RIM exactly.  Untested as of this note.

## Driver internals worth keeping

- `binder_devctl` @0x4128 (dispatch: VERSION 0x4ba0, WR @0x4278, CFG @0x4bb2,
  TXN @0x425a), `binder_thread_write` @0x2a0c (command dispatch @0x2aa0+,
  accepts BC_TRANSACTION/BC_REPLY via `r6 + 0x7fd79d00 <= 1` @0x2b1c -> txn
  handler @0x2f26).
- Command dispatch compares **full words** (RIM-encoded); the low byte is only
  used for stats.  "unknown command %d" @0x606b (EINVAL path @0x3f24).
- Error strings: "invalid size %d-%d", "invalid data ptr", "invalid offsets
  ptr", "got transaction to invalid handle".
- CFG: creates `/dev/shmem/binder_<pid>` (getpid), maps it, **peer-maps it
  into the client** (mmap_peer import) and writes the client-side address at
  cfg+0x100 — that is `mVMStart`.  `mmap()` on the fd is not supported.

## Root mechanism (device) — why root sometimes doesn't apply

`/apps/sys.android.gYABgKAOw1czN6neiAT72SGO.ns/native/system/xbin/btool` is a
**shell script** (our patched version; copy in
`notes/session12-passport-root/btool.patched`, identical to the device's).
It applies:
```
/proc/boot/pathtrust !/accounts/devuser/rootdata/launcher_patcher
/proc/boot/pathtrust !/base/bin/mod_nvram
/proc/boot/pathtrust !/base/bin/__root
```
then `slay -f sshd; /usr/sbin/sshd`, sets `usbdnet_ipaddr::169.254.0.1`, kills
the sud.py python, and **posts the "Congratulations! ... rooted!" notification**
(that notification is the user-visible signal that root applied).

It is triggered at startup via the launcher chain (the launcher_patcher runs
from the launcher).  If it did not run, `/base/bin/__root` fails with
"Operation not permitted" and there is no `/tmp/launcher_patcher.log`.
devuser cannot run `pathtrust` directly (perm denied).  The user's ritual:
**reboot and wait for the root notification**; then `/base/bin/__root` works.

## Current device state (as of this note)

- Rebooted; root re-application pending (waiting for the notification), so the
  latest libbinder is **built but not yet deployed**.
- Build: `runtime/a11-build` -> `/tmp/a11obj/qnx/libbinder.so`
  (2,402,920 B, message-offset + synthesized PING payload) — pushed to the
  device as `/tmp/newlibs5/libbinder.so` but **/tmp clears on reboot** ->
  re-push (`bbput.py <host libbinder.so> /tmp/newlibs5/libbinder.so`).
- Deploy target: `$NS/system/lib/libbcc.sha1.so` (trusted inode; `cat` in
  place as root), then trigger the probe.
- Probe: `dexdump` = `tb_pself2` (prints `UBS self=... D dsm=...`), run by
  wrapper11 with `LD_LIBRARY_PATH=$NS/ourprobe/lib:$LD_LIBRARY_PATH`.
- Runtime: user opens SkyTube (cold start) to run the wrapper probe; the
  automated LMK+invoke trigger only works while the runtime is already up.

## Exact next steps (pick up here)

1. After the root notification: reconnect (see the tunnel ritual below),
   re-push the libbinder build, `slay -f -s 9 dexdump`, deploy into
   `libbcc.sha1.so`, user opens SkyTube.
2. Read `$NS/ourprobe/run.log`: expect
   `QB wr rc00000000` + `QB wr wc...` + `QB wr rcc...` and `dsm=<nonzero>`;
   if rc != 0, the driver still rejects — next diagnostic ideas:
   - capture the driver's slog2 message (write a slog2 reader; /dev/slog2
     ring buffers read as empty via cat),
   - try flags=0 (drop TF_ACCEPT_FDS), and sender_pid/euid filled,
   - read the txn handler 0x2f26..0x3160 fully (the checks around 0x34c0).
3. Once `QB wr rc=0`: extend `tb_pself` to `checkService("manager")` /
   `listServices()` — first real round trip.
4. Cleanup for the user's runtime: the wrapper hook and `dexdump` can be
   restored from the session37 dump (`/tmp/opencode/rimlibs`, host-side
   backups) once testing is done.

## Tunnel ritual (host)

```
pkill -f Connect.jar
rm -f /tmp/bb_key /tmp/bb_key.pub
ssh-keygen -t rsa -b 4096 -f /tmp/bb_key -N ""
~/priv-research/bbndk-tools/host_10_3_1_12/win32/x86/usr/bin/blackberry-connect \
    169.254.0.1 -password 61482501 -sshPublicKey /tmp/bb_key.pub   (keep running)
```
paramiko with `disabled_algorithms={'pubkeys':['rsa-sha2-512','rsa-sha2-256']}`.
Often needs a second connect pass / longer wait.  Host scripts:
`/tmp/opencode/{bbssh.py,bbput.py,bbpull.py}`.

## Host-side artifacts (this machine; re-create if on another box)

- `/tmp/a11obj/qnx/` — the built chain (libbinder/libutils/libbase/
  libcutils/liblog/libc++); rebuild with `runtime/a11-build/build.sh` then
  `qnx-link.sh` (always run both; build.sh alone leaves the old .so).
- `/tmp/opencode/rimlibs/` — the original runtime libs (restore sources);
  `/tmp/opencode/dexdump.orig` — original dexdump.
- Repos: this note + `tools/passport-a11/*` (Passport);
  `runtime/a11-build/*`, `ws1/*` (Classic).
