#!/usr/bin/env bash
# dump_passport_android.sh - comprehensive read-only dump of the connected
# BlackBerry "oslo" Android 5.1 device (0fca:8032) into the Passport repo.
set -u
export PATH=/home/stanw47/bin/platform-tools:$PATH
D=/home/stanw47/Documents/Blackberry-Passport-Research/recon/passport-android-dump
mkdir -p "$D"/{root,system,proc,lists}
S(){ adb shell "$1" 2>&1; }

# --- root-level config files ---
for f in default.prop fstab.qcom file_contexts property_contexts seapp_contexts \
         service_contexts selinux_version sepolicy addon_verity_key verity_key \
         system_signature init.rc init.environ.rc init.usb.rc init.zygote32.rc \
         init.trace.rc init.target.rc init.oem.rc init.factory.sfi.rc init.metrics.rc \
         init.class_main.sh init.mdm.sh init.qcom.rc init.qcom.sh init.qcom.usb.rc \
         init.qcom.usb.sh init.qcom.early_boot.sh init.qcom.factory.sh \
         init.qcom.class_core.sh init.qcom.ssr.sh init.qcom.syspart_fixup.sh \
         ueventd.rc ueventd.qcom.rc; do
  S "cat /$f" > "$D/root/$f" 2>/dev/null
done

# --- system ---
S 'cat /system/build.prop' > "$D/system/build.prop" 2>&1
for d in app priv-app framework lib lib/hw lib/egl lib/modules bin xbin etc etc/firmware \
         etc/permissions vendor vendor/app vendor/lib vendor/firmware vendor/etc \
         media usr tts fonts; do
  S "ls -la /system/$d" > "$D/lists/system_$(echo $d | tr / _).txt" 2>&1
done
# recursive-ish key dirs
S 'ls -laR /system/vendor' > "$D/lists/system_vendor_R.txt" 2>&1
S 'ls -la /res' > "$D/lists/res.txt" 2>&1
S 'ls -la /sbin' > "$D/lists/sbin.txt" 2>&1
S 'ls -la /firmware' > "$D/lists/firmware.txt" 2>&1
S 'ls -la /nvram /nvram/nvuser /nvram/perm /nvram/blog' > "$D/lists/nvram.txt" 2>&1

# --- proc / kernel ---
for p in version cmdline cpuinfo partitions modules filesystems mounts misc iomem ioports \
         kallsyms config.gz; do
  S "cat /proc/$p" > "$D/proc/$(echo $p | tr / _).txt" 2>/dev/null
done
S 'cat /proc/sys/kernel/osrelease; cat /proc/sys/kernel/version' > "$D/proc/sysver.txt" 2>&1

# --- build / package / system info ---
S 'getprop' > "$D/prop.txt" 2>&1
S 'pm list packages -f' > "$D/packages.txt" 2>&1
S 'pm list features' > "$D/features.txt" 2>&1
S 'dumpsys package' > "$D/dumpsys_package.txt" 2>&1
S 'dumpsys activity' > "$D/dumpsys_activity.txt" 2>&1
S 'dumpsys window' > "$D/dumpsys_window.txt" 2>&1
S 'dumpsys telephony.registry' > "$D/dumpsys_telephony.txt" 2>&1
S 'service list' > "$D/service_list.txt" 2>&1
S 'ls -la /dev' > "$D/lists/dev.txt" 2>&1
S 'ls -la /dev/block' > "$D/lists/dev_block.txt" 2>&1
S 'getenforce' > "$D/getenforce.txt" 2>&1
S 'ls -laZ /data /data/local /data/local/tmp' > "$D/lists/data.txt" 2>&1
S 'logcat -d -t 2000' > "$D/logcat.txt" 2>&1
S 'cat /proc/last_kmsg' > "$D/last_kmsg.txt" 2>&1

echo "dump complete -> $D"
find "$D" -type f | wc -l
