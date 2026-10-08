# Session 36 — Android-on-QNX deep research: prior art, BlackBerry's trajectory,
# and the sole-OS plan

Date: 2026-10-08. Question: can we run Android as the **sole OS over the QNX
kernel** (not the BB10 container, not a Linux kernel) — using what we already
have — and what did BlackBerry/QNX actually build?

## 1. Summary answer

Yes — it is a real, productized architecture (QNX shipped it as a commercial
runtime), and it is the only route that fits: no hardware tools, no boot-chain
change, community-distributable. The work is a **source port of the Android
userland onto QNX Neutrino** (the RIM "Player" model, modernized to A11). Our
collection already holds the reference binaries and a working A11 build harness.

## 2. Prior art (sources weighed)

| work | what it is | weight |
|---|---|---|
| **RIM/BlackBerry "Player"** — PlayBook (Android 2.3), BB10 10.2.1+ (4.3, API 18, multicore) | Android userspace running natively on QNX Neutrino inside a sandbox/app player | **primary** (shipped; we hold the binaries) |
| **QNX "Runtime for APK"** (QNX SDP 6.6 docs, 2018) | *"The Dalvik virtual machine which has been ported from Linux onto the QNX Neutrino RTOS… runs natively (not emulated)"*, native `.so` run unrecompiled; integrates like any QNX UI tech (QNX CAR) | **primary** (QNX official docs) |
| **Myriad Alien Dalvik** (2011) | commercial "Android apps on non-Android platforms"; contemporaneous press said it would ship with RIM's QNX phones | medium — **not confirmed**: the BB10 specimens carry no Myriad/Alien strings; the shipped runtime reads as RIM-integrated |
| **QNX Hypervisor / QAVF** (7.x/8.x) | the *modern* commercial answer: Android as a **VM guest with its Linux kernel** (ACK + VIRTIO); docs note "Android uses a Linux kernel" | **primary** — but not available on BB10 devices |
| community | no modern Android-userspace-on-QNX project found; ours is at the frontier. Adjacent: `Psyden57/BB-PlayBook-gcc-9.3.0` (modern GCC for QNX 6.5) | medium |

## 3. Did BlackBerry try QNX-Android as the whole OS and choose Linux?

- BB10 (2013-2015) = QNX + Android **app player** (compatibility runtime), never
  a Linux or whole-Android OS.
- BlackBerry's Android phones (Priv 2015, DTEK, KEYone) = standard **Qualcomm
  Linux/Android BSPs**, not QNX-hosted Android.
- No public record of an internal "Android-as-OS-on-QNX" attempt. The technical
  reason Linux won for their Android devices is visible in our own port work:
  Android's framework + HALs expect Linux interfaces (binder driver, ashmem,
  lowmemorykiller, alarm/wakelocks…), and using the Qualcomm BSP is instant,
  while a full source port is a multi-year effort. For BB10 they owned QNX and
  built the **Player** for app compatibility only.
- QNX's own productized APK runtime (QNX CAR, etc.) proves the model, but as an
  *app runtime component*, not a phone OS. So "Android as sole OS on QNX" = our
  port at A11 scale — or a hypervisor guest, which BB10 hardware never shipped.

## 4. What we hold (RE + build assets)

- **BlackBerry's own Player binaries** (Classic repo `specimens/`, byte-exact,
  `MANIFEST.sha256`): `libbionic.so` (bionic→QNX bridge), `libandroid_runtime.so`,
  `libandroidloader.so`, `app_process`, `linker`, `servicemanager`, `mediaserver`,
  `surfaceflinger`, `libsurfaceflinger.so`, `binder` (QNX resmgr `/dev/binder`),
  `android_resmgr`, `android_launcher`, `shrimp`, `epolld`, `adbd`,
  `libbinder.so`, `liblog.so`, `libdl.so`, graphics stack
  (`libgralloc_screen`, `libframebuffer_screen`, `libhwcwindow`, `libEGL`,
  `libGLES*`, `gralloc.{Adreno,SGX,default}`, `hwcomposer.default`), HALs
  (`sensors.default`→PPS, `camera.default`→camapi, `audio.primary.default`→asound),
  **`init.cfg`** (the runtime's service/zygote config), and QNX reference libs.
- **A11 port harness** (`runtime/a11-build/`: build.sh, qnx-link.sh, test) +
  `~/android-mine` (NDK r23c + AOSP trees) + `graft/a11-frameworks-native`,
  `ref/a11_core`, `specimens/a11_*` (binder UAPI, ART APEX, hw_ref).
- **OS packaging**: our autoloader/QCFM pipeline (sessions 30-35) — the sole-OS
  delivery mechanism.

## 5. The sole-OS plan (Android userland over the stock QNX boot chain)

1. **Boot**: stock signed **IFS** (QNX kernel + startup + drivers) stays
   untouched (verified bit → no boot0 write, no hardware). The startup brings up
   the QNX system; our **`init.cfg`/scripts** (in RCFS) start the Android
   userland instead of BB10 services.
2. **Deliver**: custom **RCFS (`/base`) + user area** via an autoloader we build
   byte-exactly (`tools/autoloader/`), pre-rooted-style — distributable to any
   rooted BB10 device.
3. **Port**: the RIM model (Android processes as QNX-native ELFs over
   `libc.so.3`, `libbionic.so` interposer; binder as QNX resmgr; graphics over
   QNX Screen/img; HALs to QNX services) — workstreams WS1-WS8.
   Current frontier (session71/72): the A11 chain (`libc++/libbase/liblog/
   libcutils/libutils/libbinder`) builds, QNX-links and **loads on-device** —
   wall: `libutils` static-init crash; next milestone `ProcessState::self()` ↔
   live `/dev/binder` (present on the Passport, `1000:10011`); then
   servicemanager → zygote/ART (WS5, the biggest) → graphics/HALs → framework.
4. **Payoff**: reuses BB10's fully-optimized QNX drivers/radio/RIL — no
   hardware bring-up (the reason the user proposed this; correct).

## 6. Open items

- Alien Dalvik lineage: press says it shipped with QNX phones; no Myriad
  strings in the binaries — check `runtime_inventory`/other Player binaries for
  fingerprints (Myriad, Alien, Dalvik vendor tags).
- Mirror BlackBerry's own **init sequence** (`init.cfg`, `android_launcher`,
  `app_process` args) for the A11 equivalent.
- QNX Runtime for APK "Implementation" doc (isolation model) — fetch for the
  component/process layout.

## 7. Sources

- QNX Runtime for APK: `qnx.com/developers/docs/6.6.0.update/com.qnx.doc.apk/`
  (overview + architecture) — primary.
- QNX Hypervisor / QAVF docs (7.1/8.0) — primary.
- Wikipedia "BlackBerry 10"; RIM PlayBook press (CrackBerry); Telegraph
  2011-08-25 (Alien Dalvik + QNX phones); OSnews 2011; Myriad PR.
- Our repos: Classic `specimens/`, `runtime/`, `graft/`; sessions 22-38, 56-72;
  Passport sessions 30-35.

## 8. Addendum — the Passport QNX kernel, acquired (2026-10-08)

- Pulled from the retail (root, `__root` → `cat`): **`/proc/boot/procnto-smp-instr`**
  (696,320 B, ARM ELF, statically linked, stripped; md5
  `af654bcbe42c7ae5cc0485acc88aa975`; saved
  `~/priv-research/kernel/procnto-smp-instr-passport-E538.bin`).
- Identifies itself as **QNX Version 8.0.0**, source branch
  `svn.ott.qnx.com/product/branches/deckard/BB10_3_3/services/system/proc/…`,
  `IFS_BOARD=qc8974-rimboot-secure`, `BOOT_LOADER=RIMBOOT`, `DECKARD_DEBUG=0`.
  (The source itself is proprietary/never released; the `bb_kernel_AAO474`
  clone we hold is BlackBerry's **GPL Linux** kernel for the Priv — a different
  thing.)
- **No in-kernel Linux/Android support**: no `binder`/`ashmem`/`linux` strings.
  ⇒ Android-on-QNX is entirely **userspace** (RIM model), the kernel needs zero
  changes. (`PERFTUNE_ENABLE_ANDROID_START=yes` is a BB perf tune.)
- The kernel is the **instrumented** build (`-instr`) → QNX `tracelogger`/system
  profiler can trace the A11 chain load / the `libutils` static-init crash at
  kernel level — a real accelerator for the current wall.
