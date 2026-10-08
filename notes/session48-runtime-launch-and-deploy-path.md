# session48 — Android runtime launch architecture + writable deployment path

Date 2026-10-08. Passport retail. Follows session47 (binder integration).
Two big findings + one device-state surprise.

## 1. The runtime container is WRITABLE and on the data partition

- `/apps/sys.android.gYABgKAOw1czN6neiAT72SGO.ns` and its `native/` are
  **mode 777**, owned `apps:10011`, on `/dev/emmc/user0` (51 GB free) —
  `touch` succeeds as devuser. The scripts are 777 too.
- ⇒ The product deployment path: drop our A11 runtime files into the container
  and launch via the runtime's own mechanism — **no OS-partition/autoloader
  change needed for the payload** (autoloader only matters for OS-level
  integration later).

## 2. Launch architecture (from the runtime's own scripts + `android_launcher`)

- `scripts/launch-android.sh` (root shell) writes a PPS message:
  `/pps/services/launcher/control` ← `msg::start_system\ndat::sys.android.…ns` —
  i.e. the OS **app-launcher service** starts the namespace.
- The namespace's `sbin/android_launcher` (RIM binary) sets the environment
  (`LD_LIBRARY_PATH=/system/lib:…`, `BOOTCLASSPATH=…`), **forks
  `scripts/restart-android-core-wrapper.sh`**, then execs `/system/bin/init
  android_core`.
- `restart-android-core-wrapper.sh` → `restart-android-core.sh` →
  `start-android-core.sh`, which starts the runtime components **in order**:
  `lowmemorykiller`, `android_resmgr`, `epolld`, `logd`,
  **`binder` & waitfor /dev/binder** → … — i.e. **`/dev/binder` is RIM's
  user-space binder resmgr, started inside the player context**. That is the
  "driver" our translation layer targets, and it works only with the player's
  credentials/abilities (consistent with sessions 43–47).
- This is the natural hook for a player-context test: prepend a probe to
  `restart-android-core-wrapper.sh` (script is world-writable; the probe then
  runs with the player's uid). Original script preserved in the session37 dump;
  a hook version was staged and then **reverted** (see below).

## 3. Device state surprise

When the PPS trigger was attempted, the device turned out to run only a
**minimal process set** (`qconn`, `sshd`, `sh`, `pidin` — no app framework,
no launcher, no PPS consumers). So `launch-android.sh` ran fine
(SCRIPT_RC=0) but nothing consumes `/pps/services/launcher/control`, and no
namespace/player processes exist. On a normally-booted device the same path
will start the runtime.

Consequences:
- Player-context binder validation is deferred until the device is in its full
  OS state (or the user starts the runtime).
- The wrapper hook was reverted to the original to avoid altering retail
  behavior; the staged payload stays in
  `native/ourprobe/` (tb_pself + our whole chain, world-readable).

## 4. What is ready for the next test

- `native/ourprobe/`: our full A11 chain + `tb_pself` (player-context probe).
- Hook template (re-apply when players can start):
  prepend to `native/scripts/restart-android-core-wrapper.sh`:
  `( export LD_LIBRARY_PATH=<native/ourprobe>; cd <native/ourprobe>; ./tb_pself ) > <native/ourprobe>/run.log 2>&1 &`
  then trigger with the runtime's own `launch-android.sh` (as root) or by
  opening an Android app on a fully-booted device.

## Files/state

- Device: wrapper restored to original; `native/ourprobe/` staged (our chain +
  tb_pself); `/dev/binder` 660; minimal process set running.
- Repo: this note; tools unchanged.
