# Session 54 — app-launch + capability model, and the a11probe new-app experiment

Date: 2026-10-12. Target: retail Passport (BLACKBERRY-E538, windermere/emea,
hwid `0x87002c0a`, **secure**), live over SSH as root, Dev Mode on. Goal of this
session: pursue the "new parallel player/runtime in its own app namespace" idea
by answering the make-or-break question — **can a second, non-RIM-signed native
app inherit the `android_player` capability** — or, failing that, pin down
exactly what gates it. Outcome: the *capability* gate (`android_player` allow
list) was located and understood, but a *prerequisite* blocker was found first
(package validation), so the gate itself is not yet the binding constraint.

## TL;DR

- A brand-new parallel runtime can be built **inside QNX as a native app**; the
  only auth gate on the `android_player` capability is a **plaintext allow-list
  in `/etc/authman/sys.res`** (line 1311), filtered through `/etc/authman/sys.acl`.
- BUT the **app launcher will not even attempt a capability grant** for our probe
  because the package is **invalid to the app-asset/package layer**
  (`appassets: package not found`). The blocker is a **valid package manifest**
  (descriptor + `META-INF`), *not* the capability policy.
- Independently: the Android runtime is **already running** (`mode::daemon`) and
  **adb over TCP 5555** is reachable from the host, giving a live shell into
  Android 4.3. This is a separate, already-open foothold.

## The a11probe new-app experiment

Build (recipe from session29): `tools/session54-a11probe-app/`
- `a11probe.c` — freestanding QNX ARM ELF that prints uid/euid/gid/egid, tries
  `open()` on `/dev/emmc{user0,boot0}`, `/dev/binder`, shared fs, and does a
  `resmgr_attach()` (the session45 gate probe).
- `blackberry-tablet.xml` — the app descriptor, declaring
  `<action>access_shared,android_player</action>` and
  `<action system="true">_sys_use_consumer_push,run_native,permanent,hidden</action>`,
  entry asset `sbin/a11probe`.
- `build.sh`, `start_min.S`, `stub/` — libc stub + minimal start.

Installed at `/apps/sys.a11probe.gYABgA11PROBE00000000000.ns/` with group `10148`;
`/accounts/1000/appdata/sys.a11probe…` created with the usual app/config/pps/db
symlinks.

### Registration (accepted)

Entry persisted in the PPS registry `/pps/system/installer/registeredapps/applications`
(and `.all`), alphabetically sorted:

```
sys.a11probe.gYABgA11PROBE00000000000.ns::gYABgA11PROBE00000000000-ns,10.3.3.213,,1,<size>,websl
```

So the installer accepted the registration. The install event even propagated:

```
search.7049357  SEARCH_LR  I: Notified of 'install': 'sys.a11probe.gYABgA11PROBE00000000000.ns'
```

### Why it never launched — package is invalid

```
navigator.6639745  appassets.cpp:130  appassets: Received message get_pkg_data (… pkg_dnames:["sys.a11probe…ns"] …)
navigator.6639745  appassets.cpp:519  appassets: package not found: 'sys.a11probe…ns'
navigator.6639745                     appassets: Replying: … "err":"package not found"
```

The app-assets provider (part of `navigator`, the UI shell) can't resolve the
package to metadata because we installed **only `native/`** — no `META-INF/`,
no `public/`, no packed/`<id>.manifest`. The stock layout has three subdirs
(`META-INF`, `native`, `public`). And the per-app **catalog**
`/pps/system/installer/appdetails/applications` (28 KB, Feb 2018) contains the
real apps (incl. `android_player`'s special entries) but **not** our probe
(`grep -c a11probe` = 0). No catalog entry → no metadata → the launcher/navigator
never gets far enough to request capabilities.

> Conclusion of the experiment: **registration ≠ package validity.** To exercise
> the capability path we must produce a package the installer fully accepts
> (descriptor + `META-INF` manifest/signature block, generating an `appdetails`
> entry). Until then the `android_player` allow-list question is untestable
> end-to-end.

## Launch + capability architecture (as discovered)

### Processes
- `/base/usr/sbin/bslauncher -d /services -c /etc/bslauncher.cfg -l /var/log/bslauncher/log.txt`
  — boot-time **service** launcher (services, not apps). Log line:
  `bsl-register: Opened /dev/authman (pid 2641973)` → it talks to authman.
- `/base/sbin/launcher` (PID 6606958; args `-U 89:0,1000,1004,… -m 970 -t 10 -p -v -b sys.navigator`)
  — the **app** launcher; reads `/pps/system/installer/registeredapps/.all`
  (and `.all?delta`) and grants abilities via `procmgr_ability` /
  `cap_apply_capabilities`. Strings include `Capability %s was denied. Launch refused`
  and `Error requesting application capabilities from authman`.
- `/base/sbin/authman -U 505` (PID 2641973) — the capability/policy daemon.

### Config files
- `/etc/system/config/launcher.cfg` — `[Launcher] headless_heapsize_limit_mb=3`,
  `boot_timeout_grace_period_secs=900`; `[LaunchDelegate]` cascades/webkit;
  `[EntryPointType]`.
- `/etc/system/config/launcher_caps` — capability → **supplementary group N**:
  `access_shared`→1, `access_sys`→2, `run_when_data_locked`→6,
  `access_operational_data_domain`→7, `access_startup_data_domain`→8,
  `read_app_metadata`→user apps. `[PrivateCapabilities]` are freely requestable.
- `/etc/authman/sys.acl` — capability **definitions**. `android_player` at line 1871
  (MAC macros + a `PAB …` block of procmgr abilities with **`inherit_yes`**;
  `inherit_yes` is why the session49–51 player-context hook inherited abilities).
- `/etc/authman/sys.res` — per-capability **allow-list (the gate)**. Lines 1309–1311:
  ```
  android_player
      deny *
      allow sys.android.gYABgKAOw1czN6neiAT72SGO.ns
  ```
  Plaintext, root-writable. The only name allowed to hold `android_player` is the
  stock Google/Android package.
- `/etc/authman/authman.cfg` — loaded at startup. Defines the **PPS control
  protocol** as a per-username command whitelist and privileged dnames:
  - `[capability_prefix_paths]` — PPS paths needing the `cds` capability.
  - `[client_whitelist]` (username → commands):
    `android_root` → `default,defapp,clrapp,dyncap,setsandboxname`;
    `android_system` → `…chkcap,renapp`; `carrier_mgr` → `regprefix,load,delcap`;
    `upd` → `getcap,uninst`; `coreServices` → `dyncap`; `apps` → `chkcap`; etc.
    So capabilities are set via **`defapp` / `clrapp` / `dyncap`** messages to authman.
  - `[PrivilegedDnames]` — a list of privileged app dnnames
    (`com.secusmart.*`, `com.rim.bb.app.bb2fa.*`, WhatsApp, Viber, …).
- `authman` is a **supervised service** started by `/base/scripts/startup.sh`
  (`SERVICE_authman_HANDLER_start` → `${BASEFS}/sbin/authman -U 505`); boot log:
  `START SERVICE: authman … END SERVICE authman - took 1.383000 seconds`. The
  service framework has stop/start handlers, i.e. a supported restart path exists
  (not yet exercised).

### Grant flow (reconstructed)
1. Installer registers app → `registeredapps/*`.
2. `navigator`/`appassets` resolves package metadata (`appdetails` catalog).
3. `launcher` picks the app and asks **authman** for the app's declared caps
   (the `<action>` list from the descriptor), sending `defapp`/`clrapp`/`dyncap`.
4. authman checks the app's **dname** against `sys.res` (per-cap allow-list),
   consults `sys.acl` for the abilities, and `[PrivilegedDnames]`/whitelist.
5. On success, abilities are applied to the child via `procmgr_ability`;
   `PAB … inherit_yes` means the child's descendants keep them.

## Android runtime + adb foothold (independent discovery)

Runtime is already running in `mode::daemon` (`/pps/services/android/status` =
`state::running`, `mode::daemon`): `android_resmgr`, `binder`,
`/system/bin/servicemanager`, `zygote -Xzygote /system/bin --zygote --start-system-server`,
`com.android.systemapps`, `com.android.phone`, `com.android.providers.calendar`,
`…/native/sbin/adbd`.

Port **5555 is LISTENING**; `adb connect 169.254.0.1:5555` from the host works and
`adb shell` gives a shell inside **Android 4.3** (`ro.build.version.release=4.3`).
`adb root` is refused (`adbd cannot run as root`). The Android shell is minimal
(no `id`/`grep`/`head`/`uniq`; use `toolbox`). Host adb binary used:
`recon/2026-10-02/autoloader/ABL766 - Hn03/host/linux-x86/bin/adb`.

## Operational quirks learned

- **PPS-backed registry files** (`registeredapps/{applications,.all}`,
  `appdetails/applications`) fail `grep`/`tail`/`cp` with *"Message too long"*.
  Read by `cat <file> > /tmp/x` on-device then `sftp.get`, or pipe through `cat`.
- The device ksh shell lacks `head` (and friends); prefer `strings | grep`, or
  stage to `/tmp` and pull.

## Status / handoff

Done this session:
- a11probe built and installed; registration accepted (registry + `.all`).
- Root-caused the non-launch: **invalid package** (`appassets: package not found`),
  not the capability policy.
- Fully mapped the launch + capability architecture (bslauncher/launcher/authman,
  `sys.acl`/`sys.res`/`authman.cfg`, `launcher_caps`, `startup.sh` service).
- Confirmed the already-running Android runtime + working `adb@5555`.

Next:
1. **Make the package valid**: add `META-INF/` (+ manifest), `public/`, and a
   descriptor the installer accepts, so `appdetails` gets an entry and
   `appassets` resolves it. Likely the cleanest route to the capability test.
2. Then test the gate: back up `sys.res`, add
   `allow sys.a11probe.gYABgA11PROBE00000000000.ns` under `android_player`,
   reload authman via the supervised service path, launch, observe abilities;
   revert.
3. Parallel route: the already-proven player-context inheritance + `adb@5555`
   foothold.
