# session49 — Player-context hook PROVEN; runtime currently stopped (needs a start)

Date 2026-10-08. Passport retail. Continues session48 (launch architecture).
The goal: run our A11 probe **as the Android player** (uid 1000 + abilities) to
validate the binder driver path with real credentials.

## What was done

1. The runtime container is writable (session48), so the launch hook is a file
   edit: **prepend a probe to
   `native/scripts/restart-android-core-wrapper.sh`** (called by RIM's
   `android_launcher` at namespace start; script is world-writable).
   Hook added (runs `tb_pself` in a subshell, logs to
   `native/ourprobe/run.log`, then the original script continues unchanged).
2. Staged the full A11 chain + `tb_pself` in `native/ourprobe/` (mode 755).
3. Trigger path used: root shell writes the **official PPS messages**
   (`/pps/services/launcher/control` `start_system`/`stop_system`,
   `/pps/services/navigator/control` `invoke android://__focus__` — the same
   ones the runtime's own `launch-android.sh` / `reboot-android.sh` use), and
   the runtime's own kill mechanism `echo shrink:-20 >
   /dev/android/lowmemorykiller` (as root via `__root`).

## Result — the hook ran in the player context

After `shrink:-20` (killed the core: 119 threads → 0) + a navigator invoke,
`run.log` was written by the **player-context wrapper**:

```
=== player-context probe ===
restart-android-core-wrapper.sh[10]: ./tb_pself: cannot execute - Permission denied
probe RC=126
```

So the injection technique works end-to-end; the only problem was that
`sftp`-staged files were not executable (now `chmod 755` — fixed).

## Current device state (IMPORTANT)

- The Android runtime core is **stopped**: our `shrink:-20` killed it, and the
  OS launcher did not restart it from our root-shell PPS messages afterwards
  (the first invoke worked while the player app was still alive; later ones
  found no app to focus).
- **The hook is left installed and the probe is executable.** The next time the
  runtime starts — user opens an Android app on the device, or a normal reboot
  — `android_launcher` will run the wrapper, `tb_pself` will execute **as the
  player** and write the result to
  `/apps/sys.android.gYABgKAOw1czN6neiAT72SGO.ns/native/ourprobe/run.log`.
  That file is the first binder-with-credentials test (expect the version
  devctl to succeed and the transaction path to progress).
- To remove the hook: restore
  `native/scripts/restart-android-core-wrapper.sh` from the session37 runtime
  dump (original is also in `recon/passport-runtime-4.3/`).

## Next

1. Get the runtime started (tap an Android app / reboot) and read
   `native/ourprobe/run.log`.
2. If the version devctl succeeds as uid 1000, proceed to the write/read
   transaction path (session46 translation + session47 redirect already in
   place in the staged `libbinder.so`).
