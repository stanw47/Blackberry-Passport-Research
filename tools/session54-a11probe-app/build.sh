#!/bin/sh
# Build the session54 a11probe QNX ARM ELF (recipe proven session29).
set -e
cd "$(dirname "$0")"
mkdir -p stub
arm-none-eabi-as -o stub.o stub.s
arm-none-eabi-ld -shared -soname libc.so.3 -o stub/libc.so.3 stub.o
arm-none-eabi-as -o start_min.o start_min.S
arm-none-eabi-gcc -marm -O2 -std=gnu99 -fPIC -ffreestanding -c a11probe.c -o a11probe.o
arm-none-eabi-ld -pie -z now --dynamic-linker=/usr/lib/ldqnx.so.2 -e _start \
  -L ./stub -o a11probe start_min.o a11probe.o -l:libc.so.3
echo "built: $(pwd)/a11probe"
arm-none-eabi-readelf -l a11probe | grep -A1 INTERP
arm-none-eabi-readelf -d a11probe | grep NEEDED
