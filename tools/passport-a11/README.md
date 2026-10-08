# tools/passport-a11 — A11-on-QNX probes used on the Passport

Probe sources used on the retail Passport while bringing up the Android 11
native chain on QNX. **Canonical build harness lives in the Classic repo**
(`blackberry-research` companions):

    Classic repo: runtime/a11-build/test/build-tb.sh   (builds tb_* probes)
    Classic repo: ws1/                                 (WS1 shim source + build)
    Classic repo: sysroot/target/lib/libc.so.3         (link stub for QNX libc)
    Classic repo: binder/                              (A11 binder port + qnxinc headers)
    Classic repo: /tmp/a11obj/qnx                      (built A11 chain: libc++.so,
                                                        libutils.so, libbinder.so, ...)

Copies here are for tracking/reference; edit the originals in the Classic repo
and re-copy if they change.

## Files

| file | purpose |
|---|---|
| `tb_shim.c` | shim-only control probe (`[SHIM] init` / `A11 libs loaded`) |
| `tb_dlopen.c` | handler-first probe: installs SIGSEGV handler in main, then dlopens the chain step by step |
| `tb_cxx.cpp` | libc++ static-init probe |
| `tb_a11.c` | loads libutils+libbinder, dlsyms `ProcessState::self`, calls `operator new` (`UBS N=0x…` / RC=0) |
| `tb_qnxb.c` | prints `qnxb_ptrs[]` (link-time QNX bindings) for the write/sigaction/pthread_key_create/dladdr slots |
| `probe_binder_step.c` | step-by-step `/dev/binder` open/ioctl/mmap probe |
| `probe_binder_devctl.c` | tries the driver via QNX `devctl` with the numbers found in RIM's libbionic |
| `binder_a11.h` | A11 binder UAPI header used by the probes |
| `start.S` | probe entry point (copy of Classic `ws1/start.S`): QNX crt sequence — argc/argv/envp, `_init_libc(argc,argv,envp)`, `main`, `exit`. **Required** for `devctl`/connection paths; never use plain `b main` |
| `rim_libbionic.so` | **specimen**: RIM's 4.3 libbionic from the Passport runtime dump; contains `ioctl_binder` (the driver bridge) |
| `ioctl_binder.dis` | objdump of `ioctl_binder` (`0xf228–0xf424`) — the bridge to port |

## `ioctl_binder` observations (from the disassembly)

- Compares the request against **0xC0186201** (BINDER_WRITE_READ) and
  **0xC108620C** (ProcessState ctor setup) in the entry block.
- Copies a **24-byte** `binder_write_read` from the caller's arg and walks the
  write buffer as 32-bit commands; command words are checked against
  **0x80286300 / 0x80286301** (unsigned `(cmd + 0x7fd79d00) <= 1`) before
  entering the transaction path — i.e. RIM's BC_ command encoding for the
  4.3 wire is not the AOSP one and needs mapping.
- Builds QNX io messages with a 16-bit type field **0x106** (`_IO_DEVCTL`) and
  sends them via `MsgSend*`/`devctl` (both imported). **Equivalent to QNX
  `devctl(fd, dcmd, buf, nbytes, &info)`** — libc's devctl builds the same
  `_IO_DEVCTL` message (verified in disassembly of the Passport libc).
- RIM's libbinder command values: `IPCThreadState::joinThreadPool` writes
  **0x630B (`BC_REGISTER_LOOPER`) / 0x630C (`BC_ENTER_LOOPER`)** — same as AOSP.
  The transaction commands are compared in `ioctl_binder` against
  **0x80286300 / 0x80286301** (unsigned `cmd + 0x7fd79d00 <= 1`), i.e. the 4.3
  wire used a different direction bit for `BC_TRANSACTION`/`BC_REPLY` than the
  modern AOSP encoding — resolve the full command table when implementing the
  translation layer.

## Build (Passport)

The probes are built with `arm-none-eabi-*` (newlib) against QNX headers,
linked exactly like the working probes — **NEEDED order matters**:

    -l:libc.so.3  -l:libc++.so  -l:libc.so      (libc.so.3 FIRST)

Example (from the host, Classic repo paths abbreviated):

    B=/tmp/tbbuild
    SHIM=<classic>/ws1/build
    SR=<classic>/sysroot/target/lib
    A11OUT=/tmp/a11obj/qnx
    CFLAGS="-march=armv7-a -mfloat-abi=soft -mthumb -Os -nostdlib -fno-builtin -fpic -ffreestanding"
    LDFLAGS="-pie --dynamic-linker=/usr/lib/ldqnx.so.2 -e _start \
             --allow-shlib-undefined --unresolved-symbols=ignore-all"

    # tb_a11
    arm-none-eabi-gcc $CFLAGS -c tb_a11.c -o $B/tb_a11.o
    arm-none-eabi-ld $LDFLAGS -o $B/tb_a11 $B/start.o $B/tb_a11.o \
        -L"$A11OUT" -L"$SHIM" -L"$SR" --no-as-needed \
        -l:libc.so.3 -l:libc++.so -l:libc.so

    # binder probes (need the Classic repo binder headers)
    arm-none-eabi-gcc $CFLAGS -I<classic>/binder/include -I<classic>/binder/qnxinc \
        -c probe_binder_step.c -o $B/probe_binder_step.o
    arm-none-eabi-ld $LDFLAGS -o $B/probe_binder_step $B/start.o \
        $B/probe_binder_step.o -L"$SHIM" -L"$SR" -l:libc.so.3 -l:libc.so

`start.o` is `ws1/start.S` built with the same CFLAGS.

## Deploy + run (we use paramiko helpers on the Linux host)

    bbput.py <local> /accounts/1000/shared/misc/android/qnx/<name>
    bbssh.py "cd /accounts/1000/shared/misc/android/qnx && \
              chmod +x <name> && LD_LIBRARY_PATH=. ./<name>"

Notes:
- A probe that spins (e.g. before the NEEDED-order fix) saturates the device at
  prio 10r; kill with `slay -f -s 9 <name>`.
- `LD_DEBUG=libs|all|bindings` and `LD_BIND_NOW=1` are the main diagnostics on
  the device.
- `/dev/binder` is `1000:10011`; devuser needs a test-only `chmod 666` (via the
  root shell helper `__root`) or must run as the Android uid.
