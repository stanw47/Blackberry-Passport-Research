# Session 24 — Prototype hardening map, Diagnostics exec channel, Dirty COW (ptrace)

Date: 2026-10-06. Builds on `session22`/`session23`. Goal: re-verify the
"diagnostics root path", map the kernel/SELinux hardening, and find a real
privileged primitive on the Android prototype (oslo, Android 5.1 LMY47D,
kernel 3.4.0-grsec-g0ea3e38). Read-only; only volatile page-cache writes (all
restored). No flash / wipe / unlock.

---

## 1. Correction: the `diagnostics` service IS an exec primitive (token present)

The session23 conclusion ("FileTask = privileged file read; no token") was
wrong on every point. Re-derived from the decompiled `Diagnostics` APK and
verified live.

### 1.1 Authoritative transaction map
From `IDiagnosticService$Stub$Proxy` (`transact()` codes):

| txn | method |
|---|---|
| 1 | `send(int etype,int d1..d4,String creator,String cmds)` |
| 2 | `append(long euid, String cmds)` |
| 3 | `append_log(long euid, int dtype, String params)` |
| 4 | `open(long euid, String name, String type)` |
| 5 | `get_guid(long euid)` |
| 6 | `get_sysvars()` |
| 7 | `admin(String cmd)` |

The session23 note had `6=send 7=open`; it is actually `6=get_sysvars 7=admin`.

### 1.2 A debug token IS present
`SystemUtils.isTokenPresent()` calls native `token_service_is_token_present`
(`libddt_jni.so` → `libbb_tokenservice.so` → `/system/bin/bb_tokenserviced`,
binder service `TokenService`, pid 371). Live proof: `append_log(euid=0,
dtype=12)` and `open(...)` return normally (no "Permission denied: debug token
required"), and logcat shows "Checking client permission: uid=2000 … Permission
granted for native system process". So all token gates are open.

### 1.3 Verified arbitrary-command execution as uid 1301 (ddt)
```
service call diagnostics 3 i32 1 i32 0 i32 11 s16 '<command>'
```
`append_log(euid=1, dtype=11, params=<command>)` → `LogRunner.parse(11,params)`
→ URI `exec://<command>` → `SysTasks$ExecTask` → `Runtime.exec(command)` **in
the Diagnostics process** (`u:r:diagnostics:s0`). `dtype=12` is
`LogTypeURI` 12 = `app` (`AppInstallUninstallTask`), not exec — exec is 11.

Verified:
```
I/DCPROOF: uid=1301(ddt) gid=1301(ddt)
  groups=1000(system),1007(log),2000(shell),2900(nvram),2904(reset_cause)
  context=u:r:diagnostics:s0
```

Notes on driving it:
- `Runtime.exec(String)` splits on whitespace and does **not** honor quotes.
  Use one whitespace-free token for `sh -c`, with `${IFS}` for spaces and
  `$(...)` for substitution, e.g.
  `sh -c log${IFS}-t${IFS}TAG${IFS}"$(id)"`.
- Exfil = logcat via `/system/bin/log` (toolbox). Multiline messages arrive as
  multiple log entries. `head`/`tail` do not exist on this build.
- The parent event (`euid`) does not have to exist: the attachment fails
  ("Unable add or attach new event: 0") but the ExecTask **side effect runs**.
- Helper: `tools/session24/dcx.sh '<cmd with spaces>'`.

### 1.4 What the ddt context can and cannot do (SELinux)
- `/nvram` readable (gid `nvram`): `blog/`, `boardid/`, `prdid/`.
- `/dev/block` — **denied**: `avc: denied { search } … scontext=u:r:diagnostics
  tcontext=u:object_r:block_device tclass=dir`. No partition access.
- `/data/local/tmp` — **denied**: `avc: denied { search } … tcontext=
  u:object_r:shell_data_file tclass=dir`. No shell-data exfil; logcat only.
- `/system/bin` readable; **no setuid/setgid binaries** (`ls -l /system/bin |
  grep -E '^-..s'` empty). `/system/xbin` = `antradio_app`, `dexdump`,
  `FixPermissions.exe`, `RIDLClient.exe` (all 0755 root:shell).

### 1.5 Other corrections
- `send` returns a generated **event ID**, not an euid.
- `FileTask` (and `ManifestTask`) only `setResult(path)` after an existence
  check — **no file contents are read**. There is no privileged file-read in
  Diagnostics; the "read /dev/block via FileTask" idea is dead.
- `DataUploadTask` pins the **Thawte Premium Server CA** (expired 2020-12-31)
  as its only trust anchor and does not override hostname verification → the
  QUIP upload cannot be redirected without that CA's private key.
- `LogEncryptorAES.generateAesKey` computes a PBKDF2WithHmacSHA1 key from
  `SystemUtils.getPIN()` (`ro.nvram.prdid.pin` = `0x2ffe921c`, 1000 iters, PIN
  bytes as both password and salt) and only replaces it with keystore alias
  `ddt.ss.aes.key` if `keystore.test()==1`; here `test()` returns 3, so the
  PIN-derived key is used. Moot without a way to read `/data/ddt`.

## 2. Kernel hardening map (live)

| probe | result |
|---|---|
| `/proc/self/mem` write (RW anon page) | `-EPERM`, page unchanged |
| `process_vm_writev` (ARM `__NR` 377) | `-ENOSYS` (not wired) |
| `ptrace(PTRACE_POKEDATA)` | **works** (write lands) |
| `/proc/kallsyms` | names visible, all addresses `00000000` (kptr_restrict) |
| `kptr_restrict` / `dmesg_restrict` | not readable by shell |
| SELinux | enforcing; custom BB policy (`runas`, `*_block_device`, `diagnostics_data_file`, …) |
| `/system/bin/run-as` | **not setuid** (`0750 root:shell`) |

## 3. Dirty COW (CVE-2016-5195) via PTRACE_POKEDATA — works

Since `/proc/self/mem` and `process_vm_writev` are blocked, the remaining
`FOLL_FORCE|FOLL_WRITE` route is ptrace. Method (`tools/session24/ptracecow.c`):

1. Parent maps the target `MAP_PRIVATE|PROT_READ`, then `fork()`s a tracee.
2. Tracee spawns thread A: `while(!stop) madvise(map, len, MADV_DONTNEED)`.
3. Parent `PTRACE_ATTACH`es the tracee (main thread stops; A keeps running —
   `PTRACE_CONT(A)` returns `ESRCH`, which is fine) and hammers
   `PTRACE_POKEDATA(tracee, map+i*4, word_i)` through the stopped main thread.
4. The COW break races with A's madvise; the word lands in the file's page
   cache. Verify by re-reading the file; repeat until the payload matches.

Verified:
- `/data/local/tmp/dc_test` (shell-owned) → `BBBB…`.
- **`/system/etc/hosts`** (root:root 0644, read-only verity `/system`) → first
  16 bytes replaced with `DIRTYCOW-OK-0001`; restored to the original bytes via
  the same primitive afterwards (verified `127.0.0.1\tlocalhost`).

Page-cache only: nothing reaches flash, a reboot restores everything. This is
an arbitrary-write primitive over any file the caller can read (uid 2000 shell
can read most of `/system`).

## 4. Escalation status — no setuid binary, need a root re-exec

Dirty COW + no setuid binary means root needs a binary that a **root process
re-executes** at a triggerable time. Candidates to evaluate next:
- `dhcpcd` (root, started by netd/framework when an interface needs DHCP;
  triggerable by Wi-Fi toggle) — Dirty-COW `/system/bin/dhcpcd`, reconnect.
- `pppd`/`dun-server` (root, tethering), `hostapd` (root, softap).
- `dumpstate` via `adb bugreport` (uid/domain not yet confirmed).
- `rmt_storage` (root, likely has block-device access, but hard to restart).
- `vold` helpers (`fsck_msdos`, `e2fsck`, `sgdisk`) on mount/format events.
The exploited process runs in *its* SELinux domain, so the domain must also be
able to read block devices (or otherwise be useful). No such target confirmed
yet.

## 5. Artifacts
- `tools/session24/ptracecow.c` — working Dirty COW (ptrace) primitive.
- `tools/session24/dcx.sh` — diagnostics exec channel helper.
- `tools/session24/dirtycow_arm.c`, `memtest.c`, `ptracetest.c`,
  `pvmtest.c` — negative results / controls.
- Build: `clang --target=armv7-linux-gnueabihf -O2 -fomit-frame-pointer
  -static -nostdlib -fuse-ld=lld -Wl,-e,_start -o X X.c`

## 6. Safety
Read-only except volatile page-cache writes to two files, both restored.
No flash / wipe / unlock. The prototype has no autoloader.
