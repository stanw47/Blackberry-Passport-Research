echo ID:; id
echo LSBLK:; ls -l /dev/block/
echo LSPLAT:; ls -l /dev/block/platform/msm_sdcc.1/
echo LSBOOT:; ls -l /dev/block/bootdevice/by-name/
echo LSRPMB:; ls -l /dev/block/mmcblk0rpmb /dev/block/mmcblk0boot0 /dev/block/mmcblk0boot1
echo PARTITIONS:; cat /proc/partitions
echo Wperm:; cat /dev/null > /dev/block/bootdevice/by-name/perm; echo rc=$?
echo Wp17:; cat /dev/null > /dev/block/mmcblk0p17; echo rc=$?
echo Wmodemst1:; cat /dev/null > /dev/block/bootdevice/by-name/modemst1; echo rc=$?
echo Wnvuser:; cat /dev/null > /dev/block/bootdevice/by-name/nvuser; echo rc=$?
echo Wbcb:; cat /dev/null >> /nvram/perm/bootcontrolblock; echo rc=$?
echo DONE
