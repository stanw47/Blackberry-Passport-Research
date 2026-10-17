echo step1_start > /storage/emulated/legacy/unlock_status.txt
cat /storage/emulated/legacy/backup_bootchain_wp-power_on_secure.pkg > /dev/block/platform/msm_sdcc.1/by-name/bcota
echo step2_bcota_written >> /storage/emulated/legacy/unlock_status.txt
setprop sys.powerctl reboot,oem-0x81
echo step3_prop_set >> /storage/emulated/legacy/unlock_status.txt
