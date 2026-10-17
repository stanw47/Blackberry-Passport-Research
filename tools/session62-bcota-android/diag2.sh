dd if=/dev/block/platform/msm_sdcc.1/by-name/rcause of=/storage/emulated/legacy/rc_tail.bin bs=1048576 skip=8 count=8; echo rc=$?
ls -l /storage/emulated/legacy/rc_tail.bin
echo DONE
