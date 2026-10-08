# session50 — BREAKTHROUGH: binder driver path validated in the player context

Date 2026-10-09. Passport retail. Continues sessions 47–49 (binder integration)
and this session's trust investigation. **All work below was performed on the
live retail Passport with the user's app triggers.**

## Result (player-context devctl probe, `tb_drv`)

```
=== player-context devctl probe ===
waited 1 s for /dev/binder
open: 0x00000003
devctl VERSION rc=0x00000000 info=0x00000000 ver=0x00000007
devctl CFG     rc=0x00000016
devctl TXN     rc=0x00000016
devctl WR      rc=0x00000000
probe RC=0
```

- `open("/dev/binder")` + `BINDER_VERSION` (dcmd `0xC0046209`) **succeed** as the
  Android player uid → the driver path is alive with real credentials.
- **Driver protocol version = 7** (RIM 4.3). A11's
  `BINDER_CURRENT_PROTOCOL_VERSION` is 8 → this is exactly why the
  `ProcessState::self()` run aborted earlier. Fix = accept 7 in the port.
- `BINDER_WRITE_READ` (dcmd `0xC0186201`, 24-byte bwr) **accepted** → the
  transaction path is open for the session46 translation layer.
- CFG (`0xC108620C`) and TXN (`0xC03C620B`) return EINVAL — our guessed structs
  are wrong; CFG failure is non-fatal (AOSP logs and continues); revisit only
  if mmap/transaction memory needs it.

## The method that got us here (BB10 app exec trust)

The hard part was running *our* code in the player context. Findings:

1. **BB10 gates exec/mmap of files by per-inode security attributes** (fsecd),
   not POSIX mode/owner/ACL (all three were equalised without effect; the
   loader showed `mmap seg 0 failed errno=1`).
2. **Content replacement preserves the trust**: overwriting an existing trusted
   file **in place** (same inode) keeps its attributes. Proven by replacing
   `system/xbin/dexdump` (unused dev tool, backed up) with our probe — the
   player executed it.
3. Libraries need the same treatment: our chain's libs were placed by
   **overwriting existing trusted libs in place** (or renaming an unused
   trusted lib to the needed name, then overwriting content):
   - `system/lib/{libutils,libcutils,liblog,libbinder}.so` overwritten,
   - `system/lib/libvideoeditorplayer.so -> libc++.so`,
     `libttscompat.so -> libbase.so`,
     `libvideoeditor_core.so -> libqnxbind.so`,
   - `system/native/lib/libc.so` overwritten (shim) — **this one broke the
     runtime core start** (RIM's QNX-side binaries need it) and was restored.
4. The loader's dependency search needs the **inherited player environment**
   (`LD_LIBRARY_PATH=/system/lib:/base/...:/system/native/lib:...`) — do NOT
   override it (overriding dropped the `host_*` providers used by RIM's
   `native/lib/libc.so`).
5. `libqnxbind.so` must sit **next to the shim** (`native/lib/`) for the
   shim's NEEDED to resolve in this context.
6. Trigger: the hook lives in
   `native/scripts/restart-android-core-wrapper.sh` (world-writable); the
   probe runs **after** the core start (background, waits for `/dev/binder`);
   restarts via LMK `shrink:-20` + navigator invoke work while the runtime is
   up, otherwise the user force-closes/reopens an Android app.

All original libs were **restored from the session37 dump** after the run
(verified sizes); the runtime starts normally again (user confirmed SkyTube
launches).

## Still deployed for the next step

- `system/xbin/dexdump` = our probe (original backed up host-side and in the
  dump) — currently the raw devctl probe `tb_drv`.
- The hook in `restart-android-core-wrapper.sh` (original in the dump).
- `ourprobe/` dir with the full A11 chain (unused by the current probe).

## Next

1. Patch the qnx-linked A11 libbinder to accept **protocol 7** (and tolerate
   the CFG EINVAL), rebuild, and re-run `tb_pself` in the player context.
2. Then the first real **transaction**: `ProcessState::self()` →
   `getService` via our 32<->64 translation (session46) against the live
   servicemanager.
3. Fix or drop the CFG/TXN structs only if needed.
