# Session 56 — Android-on-QNX: era learnings, foreign-kernel porting, and a staged port process

Date 2026-10-13. Question (user): what can we learn about QNX/BB10 from this
era, about porting an OS to a kernel where it "shouldn't" run, and what process
lets us port bits of Android to the QNX userspace until we have working Android
— then sole-OS, with an update channel.

Scope: strategy + method. Grounded in our own sessions 36–55 (A11 chain, binder,
player) plus era documentation and prior art.

---

## Part 1 — What QNX / BB10 actually are (the era)

### 1.1 BB10 = QNX Neutrino 6.5.0 (the "BBX" fork)
- BB10 is QNX Neutrino 6.5.0 (announced as "BBX", 2011), acquired by RIM in 2010.
- The **kernel is a microkernel** (`procnto` = microkernel + process manager).
  Everything else — drivers, filesystems, networking — is a **user-space
  resource manager**.
- IPC is **synchronous message passing** (`MsgSend/MsgReceive/MsgReply`), with
  POSIX IPC, shared memory, pipes, FIFOs built on top. A resource manager takes a
  pathname prefix and answers POSIX `open/read/write` + `devctl()`.
- There is an **instrumented microkernel** (`procnto-instr`) with real-time
  **tracing/profiling** — our device runs exactly this (`procnto-smp-instr`,
  session36 §8). This is the on-device "logcat/trace" window.
- QNX is POSIX (PSE54-ish) but **not Linux**: no `fork()` (use `posix_spawn`),
  tiny `/proc`, no `strace` (use `tracelogger`/`traceprinter`), no System V IPC.

### 1.2 BB10's Android runtime — the "Player" (this is the whole precedent)
- BB10 shipped **Android 4.3 (API 18)** userspace running **natively on QNX**
  (not emulated), in a sandbox — internally the **"Player"**. Our device's
  player: `sys.android.gYABgKAOw1czN6neiAT72SGO.ns`, build `player-2.0.0`,
  `full_playbook-user 4.3`, built by `ec_agent` (Apr 2016). Android apps limited
  to 2 cores.
- The player binaries we hold (Classic repo `specimens/`, byte-exact) show the
  model precisely:
  - `libbionic.so` — the **bionic→QNX bridge** (Android's libc mapped onto QNX),
  - `binder` — a QNX **resource manager at `/dev/binder`** (Android's IPC as a
    QNX resmgr), plus `android_resmgr`,
  - `android_launcher` / `init.cfg` / `app_process` — the runtime host,
  - `servicemanager`, `surfaceflinger`, `libsurfaceflinger.so`, `mediaserver`,
  - graphics: `libgralloc_screen`, `libframebuffer_screen`, `libhwcwindow`,
    `libEGL`, `gralloc.{Adreno,SGX,default}`, `hwcomposer.default` — i.e.
    **gralloc/HWC over QNX Screen**,
  - HALs: `sensors.default`→PPS, `camera.default`→`camapi`,
    `audio.primary.default`→`asound`,
  - `shrimp`, `epolld`, `adbd`, `liblog.so`, `libdl.so`.
- So the mapping is a **known, shipped, byte-level reference** — not a guess.

### 1.3 QNX productized the same idea commercially
- **QNX "Runtime for APK"** (QNX SDP 6.6, docs): *"The Dalvik virtual machine
  which has been ported from Linux onto the QNX Neutrino RTOS… runs natively
  (not emulated)"*; APKs run unmodified except telephony/SMS and Google Play
  Services. QNX CAR 2.1 ran Android apps in a container.
- **Myriad Alien Dalvik** (2011): Android apps on MeeGo and even an iPad demo —
  Dalvik portability across kernels proven commercially.
- **QNX Hypervisor / QAVF** (7.x/8.x) is the *modern* answer (Android as a VM
  guest **with its Linux kernel**, ACK + VIRTIO) — but BB10-era hardware never
  shipped it. **"Android uses a Linux kernel"** is the hypervisor's stance; the
  *native* QNX-Android is the userspace port (our route).

### 1.4 What this era teaches
1. **The kernel is not what makes Android "Android"** — the native layer + ART +
   framework are. Every shipped example is a **userspace port**, never
   unmodified Android.
2. QNX's microkernel + resource-manager + message-passing model is **friendly**
   to a userspace port (POSIX + clean IPC + user-space drivers).
3. The **hard, device-specific glue** (binder ABI, HALs, graphics) is where the
   effort is — and RIM already solved it once, on this exact silicon family.

---

## Part 2 — Porting an OS to a "wrong" kernel: what's known

### 2.1 The precedent set (all userspace ports, all real)
| project | what | host kernel | lesson |
|---|---|---|---|
| RIM Player (4.3) | Android userspace on QNX | QNX Neutrino | shipped to millions; our template |
| QNX Runtime for APK | Dalvik ported to QNX | QNX | commercial product |
| Myriad Alien Dalvik | Android apps on MeeGo/iPad | Linux/XNU | Dalvik is portable |
| **libhybris** | load **bionic** drivers in **glibc** processes | Linux (glibc) | the interposer technique |
| **Halium** | standardized Android↔distro abstraction | Linux | the "containerized Android system" pattern |
| **binder-for-linux** | binder IPC on Ubuntu | Linux | binder is portable given a driver + client |
| Dalvik Porting Guide | VM port | any UNIX | non-portable part = **JNI call bridge (FFI)** |

### 2.2 The theory of "porting OS A onto kernel B"
Android = **Linux kernel glue** + **userspace** (bionic, native services,
binder, HALs, ART, framework). Porting = **replace the kernel glue with host-OS
glue**, bottom-up:

| Android assumes (Linux) | QNX equivalent | our vehicle |
|---|---|---|
| **bionic** libc + syscalls | `libc.so.3` (QNX) | **libc shim** (`libbionic`/libhybris-style interposer) |
| **binder** driver + `/dev/binder` | QNX IPC / a QNX resmgr | **binder-as-resmgr** (RIM's) + a native client |
| **ashmem / memfd** | QNX shared memory | shm wrapper |
| **gralloc / HWC** | QNX **Screen** (client/server) | gralloc/HWC HAL over Screen |
| **HALs** | QNX services (PPS/camapi/asound/…) | thin HAL bridges |
| **props/log** | PPS / slog2 | property + log bridges |
| **lowmemorykiller/PSI/ION/seccomp/netlink** | — | stub or map; the "leak" surface |
| **ART futex/timers/memfd/perf** | QNX pthreads/timers | the make-or-break workstream |

**libhybris is the closest published method**: load bionic-compiled libraries
into a foreign-libc process via a modified Android linker + `android_dlsym`, and
run a stripped Android system as a *container*. We invert it: our *host* is QNX,
and we run the Android userspace as the system.

### 2.3 The honest difficulty (why RIM's 4.3 was already "sluggish")
- **ART (WS5)** is the cliff: A11's ART assumes Linux futex/timers/memfd/ashmem/
  perf/seccomp.
- **Graphics**: A11 wants gralloc4/HWC2/dmabuf; Screen can back it (RIM did
  gralloc1/HWC1) but compat/perf will hurt.
- **Kernel assumptions leak** (`/proc`, `/sys`, ioctls) — apps that poke Linux
  internals fail.
- No GMS. Expect **LineageOS-without-Play** quality, not a modern daily driver.
- **This is a multi-year effort** in the general case. So: stage it, container
  first, sole-OS last.

---

## Part 3 — The port process (incremental, "bits until it works")

### 3.0 Principle
Port **bottom-up by dependency**, one component at a time, each proven on-device
**in the container** before the next. Every step = *build (QNX) → link (shim) →
run (container) → trace (procnto-instr) → fix → repeat*. Nothing touches the boot
until the runtime is proven.

### 3.1 The dependency ladder (what to port, in order)
```
WS0  harness            container + tracelogger + logging        [have]
WS1  libc bridge        bionic→libc.so.3 shim, pthread redirect  [have, session38]
WS2  native libs        libc++ libbase liblog libcutils libutils [have, session38/51]
WS3  binder             native client → RIM /dev/binder ABI      [blocked: EINVAL]
WS4  servicemanager     register/get services                    [next]
WS5  ART / zygote       app_process, dex2oat, the runtime        [biggest]
WS6  graphics           SurfaceFlinger + gralloc/HWC over Screen [big]
WS7  HALs               sensors/camera/audio/telephony bridges   [medium]
WS8  framework          system_server + A11 framework (APEX)     [big]
WS9  launcher/UI        the home app + input                     [end]
WS10 sole-OS            boot starts the runtime, not BB10        [end]
WS11 updates            autoloader/QCFM delivery of the runtime  [end]
```

### 3.2 The per-component port loop (the actual method)
1. **Build for QNX** (ntoarmv7 / our A11 build harness), `-DBINDER_IPC_32BIT=1`
   (RIM wire = 32-bit), `LOG_NDEBUG=1`.
2. **Link against the shim** with the correct NEEDED order —
   `-l:libc.so.3 -l:libc++.so -l:libc.so` (session38: wrong order = silent
   load-time spin).
3. **Deploy in the container** into *unused trusted inodes* (content-swapped so
   path-trust is preserved), exposed via a search-path dir with **hard links**
   (session51). Never break the real system libs the runtime core NEEDs.
4. **Run + trace**: capture the component's output AND the kernel trace
   (`procnto-instr`). The driver/core logs go to slog2 — read them via the slog2
   reader, or the component's own logging helper.
5. **Fix the specific divergence** (each fix is a documented shim rule): e.g.
   session51's fixes = pthread redirect (libc.so.3 exports shadow bionic),
   `getrlimit` 16-vs-8-byte struct smash, pthread_once over futex-emulation.
6. **Regression**: rerun the whole ladder (a lower component can break an upper
   one). Only advance when the current rung is green on-device.

### 3.3 Telemetry ("the emulator + logcat" equivalent)
- **Container = the virtual device** (full, matched, unbrickable).
- **`procnto-instr` + tracelogger** = the kernel-level trace (sees syscalls, the
  binder devctl, scheduling, the crash) — better than printf.
- **slog2** = the runtime/driver log channel (ring buffers; needs a reader).
- **QNX `pidin`, `hogs`** = process/CPU inspection.
- (Note: `pdebug` attach is blocked by BB10's `/proc` policy — session51 — so
  kernel tracing + slog2 are the windows, not a live debugger.)
- A **host QNX VM** (QNX SDP + QEMU, ARM or x86) is a *partial* extra harness:
  good for pure-userspace logic, but it has no RIM binder driver or device HALs,
  so it cannot validate the hard glue.

### 3.4 The unblock to start with (WS3)
RIM's `system/bin/binder` **has symbols** (`binder_devctl`@0x4128,
`binder_thread_write`@0x2a0c, `IPCThreadState::writeTransactionData`@0x22cee).
The `EINVAL` is a shim guessing at an unknown ABI. **Method:** implement the
client to RIM's *exact* wire format (copy their struct/pointer conventions),
not translate AOSP's. This is the "new player" advantage: stop guessing, follow
a known spec.

---

## Part 4 — Sole-OS (running *instead of* BB10)

Session36 §10: same kernel, same signed boot chain — only **who starts what**
changes.
1. **Boot scripts start the runtime** (our `init.cfg`/launcher) as the system
   sequence instead of BB10 services.
2. **Display ownership** moves to SurfaceFlinger (fullscreen Screen window/display
   API) instead of a BB10 window.
3. **Input** → Screen/`devc-serusb`/touch → Android InputFlinger.
4. **Host services** (radio/RIL/modem/power/security) stay **headless** and keep
   the same bridges — they're the optimized part; reuse, don't port.
5. **Storage**: `/data`, `/cache`, `/system` mapped; `vold` replaced by a QNX fs
   server.
6. **UI**: Android launcher/`system_server` owns the UI; BB10 apps/UI removed.
7. **IFS stays untouched** (verified) — the kernel is *supposed* to be QNX.

Phases: (a) container → (b) kiosk/hybrid (autostart the runtime fullscreen;
~80% of the user experience) → (c) true sole-OS.

## Part 5 — The update channel
- **Delivery = the user area** (QCFM/autoloader), IFS untouched. The same
  channel the balika/rooted autoloaders use. A **custom autoloader carrying the
  runtime + `system`** *is* an update.
- Build it byte-exactly with our `tools/autoloader/` (cap.exe + ver2 table +
  `.signed` streams; `qcfm_pack.py`/`make_autoloader.py`, session32/33).
- In-OS OTA: a QNX-native updater service that stages the new `system`/runtime
  into the user area and reboots — the mirror of BB10's updater.
- Note: our autoloaders flash fine *without* the signature trailer (the CRC
  couldn't be recovered) — so a custom update channel is already proven to load.

---

## Part 6 — Milestones & risks

**Milestones (each = an on-device green):**
1. WS3 binder `EINVAL` fixed → first real txn lands.
2. WS4 servicemanager up → `defaultServiceManager()` returns a real handle.
3. WS5 zygote runs one A11 app.
4. WS6 SurfaceFlinger draws → *you see Android* (the "look/concept" moment).
5. WS8 framework boots → launcher.
6. WS10 sole-OS boot → BB10 replaced.
7. WS11 update flashes a new runtime.

**Risks:** ART on QNX pthreads (WS5) is make-or-break; graphics compat/perf;
Linux-assumption leaks; no GMS; permanent bespoke AOSP fork. Mitigation =
container-first staging + the instrumented-kernel trace + RIM's binaries as the
byte-level spec.

**The upside:** reuse BB10's fully-optimized QNX drivers/radio/RIL — no hardware
bring-up — and a distributable image (autoloader) for any rooted BB10 device.

---

## Appendix — source anchors
- Our sessions: 36 (Android-on-QNX research, kernel), 37 (runtime dump),
  38/51 (A11 chain runs; binder), 30/33 (components; "content yes, chain no"),
  15 (NV pose), 53 (conversion set).
- Classic repo: `specimens/` (player binaries), `runtime/a11-build/`, `ws1/`,
  `graft/`.
- Era docs: QNX Neutrino System Architecture (microkernel/resource managers);
  QNX Runtime for APK (6.6); QNX Porting Guide; QNX Screen (graphics);
  QNX Hypervisor/QAVF (Android integration); Dalvik Porting Guide;
  Myriad Alien Dalvik (2011); libhybris/Halium; binder-for-linux.
