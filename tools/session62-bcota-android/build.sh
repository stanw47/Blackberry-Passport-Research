#!/bin/sh
# Build the session62 bcota/BCB reader for the oslo/Passport Android prototype.
# Syscall-only, static, freestanding (same recipe as session61).
set -e
cd "$(dirname "$0")"

CC=${CC:-arm-none-eabi-gcc}

$CC -marm -O2 -std=gnu99 -ffreestanding -fno-builtin -fno-stack-protector \
    -nostdlib -nostartfiles -Wl,-e,_start \
    -o bcota_read bcota_read.c

echo "built: $(pwd)/bcota_read"
"${PREFIX:-arm-none-eabi-}readelf" -h bcota_read | sed -n '1,20p'

# ---------------------------------------------------------------------------
# Deploy (session26 delivery: vold runs helpers as root in u:r:vold:s0):
#   1. push bcota_read to the device, Dirty-COW-patch /system/bin/fsck_msdos
#      with it (tools/session26/ptracecow2.c).
#   2. physically eject + reinsert the microSD -> vold -> fsck_msdos (our
#      payload) runs as root in the vold domain. Payload forks; parent exits 0.
#   3. pull results:
#        adb pull /storage/emulated/legacy/bc_read_log.txt
#        adb pull /storage/emulated/legacy/p2_bcota.bin
#        adb pull /storage/emulated/legacy/partitions_live.txt
#   Read-only; a reboot restores the pristine fsck_msdos.
# ---------------------------------------------------------------------------
