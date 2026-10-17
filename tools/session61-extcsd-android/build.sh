#!/bin/sh
# Build the session61 EXT_CSD reader for the oslo/Passport Android prototype.
# Syscall-only, static, no libc -> link freestanding with the bare-metal ARM
# toolchain (same family as tools/session29-extcsd/build.sh, but no QNX stub:
# this runs on Android/Linux, so a plain static ELF called by `execve` works).
set -e
cd "$(dirname "$0")"

CC=${CC:-arm-none-eabi-gcc}

$CC -marm -O2 -std=gnu99 -ffreestanding -fno-builtin -fno-stack-protector \
    -nostdlib -nostartfiles -Wl,-e,_start \
    -o extcsd_read extcsd_read.c

echo "built: $(pwd)/extcsd_read"
"${PREFIX:-arm-none-eabi-}readelf" -h extcsd_read | sed -n '1,20p'

# ---------------------------------------------------------------------------
# Deploy (session26 delivery: vold runs helpers as root in u:r:vold:s0):
#   1. Dirty-COW-patch /system/bin/fsck_msdos with ./extcsd_read
#      (tools/session26/ptracecow2.c, binary-payload variant).
#   2. Trigger vold to mount the vfat SD card: physically eject + reinsert it.
#      vold -> fsck_msdos (our payload) runs as root in the vold domain.
#      The payload forks; the parent exits 0 so the mount proceeds.
#   3. Results (vold domain, root) land on shared storage:
#        adb pull /storage/emulated/legacy/extcsd_j61.txt
#        adb pull /storage/emulated/legacy/extcsd_debugfs.bin   # if debugfs worked
#        adb pull /storage/emulated/legacy/extcsd_ioctl.bin     # if ioctl worked
#   Byte [179] in the .bin is PARTITION_CONFIG. Nothing is written to the eMMC.
#   A reboot restores the pristine fsck_msdos (page-cache patch only).
# ---------------------------------------------------------------------------
