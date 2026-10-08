# Session 37 — Full BB10 Android 4.3 runtime dumped from the retail Passport

Date: 2026-10-08. Goal: capture the complete Android runtime from the phone (stop
relying on the Classic's partial specimens) as the reference set for the
A11-on-QNX port.

## Dumped (via root `tar` on-device, pulled over SSH)

| artifact | size | contents |
|---|---|---|
| `passport_player43.tar` | 169,441,280 | entire `/apps/sys.android.gYABgKAOw1czN6neiAT72SGO.ns` (1,179 files) |
| `passport_var_android.tar` | 2,365,440 | `/var/android` (incl. a getroot leftover, `recovery`, `xxx`) |
| `passport_appdata_android.tar` | 3,543,040 | `/accounts/1000/appdata/sys.android…` (`data/`, `logs/`, `sharewith/`, `tmp/`; sockets skipped) |
| `passport_libc.so.3` | 598,616 | QNX libc (md5 `736f43e4…`) — **differs from the Classic/sysroot copy (`46311856…`)** |
| `passport_libpps.so.1` | 29,392 | QNX PPS lib |

Container layout: `native/{init.cfg (27,835 B), blackberry-tablet.xml (18,663 B),
default.cfg, autolaunch.cfg, sbin/, scripts/, system/, images/}` +
`public/` + `META-INF/`. `native/system/` = full AOSP-4.3 userland
(`bin/` 164 libs+daemons, `lib/` 164 `.so`, `framework/` jars+odex, `etc/`,
`fonts/`, `media/`, `usr/`, `tts/`, `app/`).

## Cross-check vs the Classic specimens

- **57 of 61 comparable files are byte-identical** (app_process, dalvikvm, init,
  libdvm 941,764, libutils, libbinder, libandroid_runtime 790,264, libui, …).
  ⇒ the BB10 4.3 runtime is the **same across devices**; the Classic A11 work
  applies directly.
- Remaining diffs are cross-version name collisions (A11 jars, A11 gralloc ref)
  and `libdl.so` (Passport 5,316 B bionic stub vs the Classic pull 9,292 B).
- **Only device delta found so far: the QNX `libc.so.3` build.**

## Locations

- `~/priv-research/working/passport-player43/` — tars, `extracted/` (169 MB),
  `player43.manifest.md5` (1,179 entries).
- Ventoy backup: `/media/stanw47/Ventoy/BlackBerry-Passport-Research/android-runtime-4.3/`.

## Paused A11 probe state (for when we resume)

On the Passport: `tb_shim` runs; `tb_cxx`/`tb_a11` **crash pre-main** — SIGSEGV
at `libc.so.3+0x49f7c` (a function entry `push {r3,r4,r5,lr}` with a wild SP,
ref `0x0f8c0f70`). The QNX loader trace shows libc++/the WS1 shim loading and
relocations binding, then the fault before `main` (probe handler never
installed). Prime suspect: the Passport's different `libc.so.3` build vs the
Classic-built shim/chain. Next step: identify the function at `0x49f7c` and
check shim/libc interactions (or reproduce with a handler-first probe that
`dlopen`s libc++.so).
