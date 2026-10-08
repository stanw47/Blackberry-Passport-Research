#!/bin/sh
# Build a BB10/QNX ARM userland tool with host arm-none-eabi (recipe proven 2026-10-08, session29).
#
# Why the pieces:
#  - Device libc.so.3 is stripped (no .dynsym) -> the linker cannot resolve
#    symbols against it. Solution: a local STUB shared lib with SONAME
#    "libc.so.3" defines the needed symbols for link time; at runtime the QNX
#    loader resolves the JUMP_SLOT/GLOB_DAT relocations by name against the real
#    /usr/lib/libc.so.3.
#  - Minimal _start: only _init_libc(argc,argv,envp) -> main -> exit.
#    Do NOT call _preinit_array/_init_array with argc/argv -- the QNX crt calls
#    them with (array_start, array_end) pointers; calling with argc/argv jumps
#    into ldqnx.so.2's iterator and crashes. Tools without constructors don't
#    need them at all.
#  - -z now (BIND_NOW): avoids the PLT0 lazy-binding path. arm-none-eabi-ld
#    merges .got/.got.plt, leaving PLT0's computed GOT base != DT_PLTGOT; the
#    QNX loader then trips on the lazy resolver. Eager binding is unaffected.
set -e
cd "$(dirname "$0")"
mkdir -p stub
arm-none-eabi-as -o stub.o stub.s
arm-none-eabi-ld -shared -soname libc.so.3 -o stub/libc.so.3 stub.o
arm-none-eabi-as -o start_min.o start_min.S
arm-none-eabi-gcc -marm -O2 -std=gnu99 -fPIC -ffreestanding -c extcsd_probe.c -o extcsd_probe.o
arm-none-eabi-ld -pie -z now --dynamic-linker=/usr/lib/ldqnx.so.2 -e _start \
  -L ./stub -o extcsd_probe start_min.o extcsd_probe.o -l:libc.so.3
echo "built: $(pwd)/extcsd_probe"
