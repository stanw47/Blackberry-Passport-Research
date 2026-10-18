#!/system/bin/sh
# read SELinux context of interesting processes -> /storage/emulated/legacy/ctx.txt
OUT=/storage/emulated/legacy/ctx.txt
{
echo "== process contexts =="
for d in /proc/[0-9]*; do
  p=${d#/proc/}
  c=$(cat $d/comm 2>/dev/null)
  case "$c" in
    battery_monitor|profiler_daemon|qosmgr|qrngd|qrngp|pfm|nqs|ml_daemon|sh|logwrapper|amploader|wcnss_filter|mm-gesture-daemon|ipblacklist|vtnvfsd|vold|netd|init|rmt_storage|nvramd|qseecomd|thermal-engine|mpdecision|sensors|healthd|servicemanager|surfaceflinger|debuggerd|installd|keystore|drmserver|mediaserver|rild|mm-qcamera-daemon|time_daemon|logd|sdcard|fsck_msdos|uncrypt|bootanimation|charger_monitor|stp_server|bkup_bc_update)
      x=$(cat $d/attr/current 2>/dev/null)
      echo "  $p $c -> $x"
      ;;
  esac
done
echo "== done =="
} > $OUT 2>&1
