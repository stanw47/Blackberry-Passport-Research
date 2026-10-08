# Passport BB10 Android 4.3 runtime — full dump (2026-10-08)

Dumped from the retail Passport (Windermere EMEA, `BLACKBERRY-E538`, rooted)
via `/base/bin/__root` + on-device `tar`, pulled over SSH. See
`notes/session37-runtime43-dump.md` for the full write-up.

## Contents

| file | size | md5 | notes |
|---|---|---|---|
| `passport_player43.tar.part-00` | 94,371,840 | `abcde2ca…` | split part 1/2 of the runtime container tar |
| `passport_player43.tar.part-01` | 75,069,440 | `158607c3…` | split part 2/2 |
| `passport_var_android.tar` | 2,365,440 | `44ff2258…` | `/var/android` |
| `passport_appdata_android.tar` | 3,543,040 | `4ce56b96…` | runtime appdata (`data/logs/sharewith/tmp`) |
| `passport_libc.so.3` | 598,616 | `736f43e4…` | QNX libc (differs from the Classic/sysroot copy) |
| `passport_libpps.so.1` | 29,392 | `334dc187…` | QNX PPS lib |
| `player43.manifest.md5` | 153,707 | `ae7bc933…` | md5 of all 1,179 extracted container files |

Reassemble the container tar:

```
cat passport_player43.tar.part-* > passport_player43.tar
tar -xf passport_player43.tar
```

Container = `/apps/sys.android.gYABgKAOw1czN6neiAT72SGO.ns`:
`native/{init.cfg, blackberry-tablet.xml, default.cfg, autolaunch.cfg, sbin/,
scripts/, system/, images/}` + `public/` + `META-INF/`; `native/system/` is the
full AOSP-4.3 userland (bin, lib, framework jars+odex, etc, fonts, media, usr,
tts, app).

## Cross-check

57 of 61 comparable binaries are byte-identical to the Classic specimens
(`Blackberry-Classic-Research/specimens`): the BB10 4.3 runtime is the same
across devices. Only device delta found: the QNX `libc.so.3` build.
