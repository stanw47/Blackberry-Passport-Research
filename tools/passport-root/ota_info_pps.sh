#!/bin/sh

##############################################################################
## Create the data for a new session                                        ##
##############################################################################
new_session_data()
{
	typeset status time_begin time_end os_old os_new radio_old radio_new

	DATA_CURRENT=""
	status=${STATUS_INPROGRESS}
	time_begin=$(date)
	time_end="0"
	if [[ -f "/.rootfs.os.version" ]]; then
		os_old=$(< /.rootfs.os.version)
	else
		os_old="0.0.0.0"
	fi
	os_new="0.0.0.0"
	if [[ -f "/.rootfs.radio.version" ]]; then
		radio_old=$(< /.rootfs.radio.version)
	else
		radio_old="0.0.0.0"
	fi
	radio_new="0.0.0.0"

	DATA_CURRENT="S1_${E_STATUS}::${status}\nS1_${E_TIME_BEGIN}::${time_begin}\nS1_${E_TIME_END}::${time_end}\nS1_${E_OS_OLD}::${os_old}\nS1_${E_OS_NEW}::${os_new}\nS1_${E_RADIO_OLD}::${radio_old}\nS1_${E_RADIO_NEW}::${radio_new}\n"
}

##############################################################################
## Rotate the data of the existing sessions                                 ##
##############################################################################
rotate_session_data()
{
	typeset count session_id status time_begin time_end os_old os_new radio_old radio_new

	DATA_HISTORY=""
	if (( ${1} >= ${MAX_SESSION_COUNT} )); then
		count=$((MAX_SESSION_COUNT-1))
	else
		count=${1}
	fi

	while (( ${count} >= 1 )); do
		status=$(PPS_VALUE ${PPS_OBJ} "S${count}_${E_STATUS}")
		time_begin=$(PPS_VALUE ${PPS_OBJ} "S${count}_${E_TIME_BEGIN}")
		time_end=$(PPS_VALUE ${PPS_OBJ} "S${count}_${E_TIME_END}")
		os_old=$(PPS_VALUE ${PPS_OBJ} "S${count}_${E_OS_OLD}")
		os_new=$(PPS_VALUE ${PPS_OBJ} "S${count}_${E_OS_NEW}")
		radio_old=$(PPS_VALUE ${PPS_OBJ} "S${count}_${E_RADIO_OLD}")
		radio_new=$(PPS_VALUE ${PPS_OBJ} "S${count}_${E_RADIO_NEW}")

		session_id=$((count+1))
		DATA_HISTORY=${DATA_HISTORY}"S${session_id}_${E_STATUS}::${status}\nS${session_id}_${E_TIME_BEGIN}::${time_begin}\nS${session_id}_${E_TIME_END}::${time_end}\nS${session_id}_${E_OS_OLD}::${os_old}\nS${session_id}_${E_OS_NEW}::${os_new}\nS${session_id}_${E_RADIO_OLD}::${radio_old}\nS${session_id}_${E_RADIO_NEW}::${radio_new}\n"

		count=$((count-1))
	done
}

##############################################################################
## Called after the new image is downloaded but before device reboots.      ##
## Initialize a new session and put the info in the pps object; rotate the  ##
## data of the old sessions if there are any                                ##
##############################################################################
update_ota_pps_preboot()
{
	typeset session_cnt

	session_cnt=$(PPS_VALUE ${PPS_OBJ} ${E_SESSION_COUNT})
	if [[ -z ${session_cnt} ]]; then
		# First time OTA on the device
		session_cnt=1
	else
		# Rotate the history data; current session will be in S1.
		rotate_session_data ${session_cnt}
		if (( ${session_cnt} < ${MAX_SESSION_COUNT} )); then
			session_cnt=$((session_cnt+1))
		fi
	fi

	# Generate the data for current (new) session
	new_session_data

	# Update pps object
	echo "${E_SESSION_COUNT}::${session_cnt}\n"${DATA_CURRENT}${DATA_HISTORY} >> ${PPS_OBJ}
}

##############################################################################
## Called after final sanity check (the upgrade/rollabck is done)           ##
## Update the destination OS/radio version, end time, and status in the     ##
## pps object                                                               ##
##############################################################################
update_ota_pps_final()
{
	typeset session_cnt status time_end os_old os_new radio_old radio_new

	session_cnt=$(PPS_VALUE ${PPS_OBJ} ${E_SESSION_COUNT})
	if [[ -z ${session_cnt} ]]; then
		# Not upgrading (the device hasn't ever been upgraded before)
		return 0;
	fi

	status=$(PPS_VALUE ${PPS_OBJ} "S1_${E_STATUS}")
	if [[ -z ${status} || ${status} != ${STATUS_INPROGRESS} ]]; then
		# Currently the device is not under upgrade/rollback. Null status shouldn't happen
		return 0;
	fi

	time_end=$(date)
	if [[ -f "${BASEFS}/etc/os.version" ]]; then
		os_new=$(< "${BASEFS}/etc/os.version")
	else
		os_new="0.0.0.0"
	fi
	if [[ -f "${RADIOFS}/etc/radio.version" ]]; then
		radio_new=$(< "${RADIOFS}/etc/radio.version")
	else
		radio_new="0.0.0.0"
	fi

	os_old=$(PPS_VALUE ${PPS_OBJ} "S1_${E_OS_OLD}")
	radio_old=$(PPS_VALUE ${PPS_OBJ} "S1_${E_RADIO_OLD}")
	if [[ ${os_old} == ${os_new} && ${radio_old} == ${radio_new} ]]; then
		status=${STATUS_ROLLBACK}
	else
		status=${STATUS_UPGRADED}
	fi

	echo "S1_${E_STATUS}::${status}\nS1_${E_TIME_END}::${time_end}\nS1_${E_OS_NEW}::${os_new}\nS1_${E_RADIO_NEW}::${radio_new}\n" >> ${PPS_OBJ}
}

##############################################################################
## Main                                                                     ##
##############################################################################
if [ -z "${__DEFINED_COMMON_SH__}" ]; then
	. ${BASEFS}/scripts/common.sh
fi

PPS_OBJ="/pps/system/ota/otainfo"
MAX_SESSION_COUNT="5"

# Entry names in pps object
E_SESSION_COUNT="SessionCount"
E_STATUS="Status"
E_TIME_BEGIN="TimeBegin"
E_TIME_END="TimeEnd"
E_OS_OLD="OsOld"
E_OS_NEW="OsNew"
E_RADIO_OLD="RadioOld"
E_RADIO_NEW="RadioNew"

# Constants (values for pps entries)
STATUS_INPROGRESS="inprogress"
STATUS_UPGRADED="upgraded"
STATUS_ROLLBACK="rollback"

# Buffers
DATA_HISTORY=""
DATA_CURRENT=""

# Places to invoke this script:
# 1. In installer, after the new image is installed and before resetting device.
#    The script will prepare initial values for the new OTA session in the pps
#    object. In this case, $1 is "preboot".
# 2. In startup.sh, as early as possible but after pps service is launched. The
#    purpose is the same as item 1. If the device is upgraded from an early
#    version that doesn't support otainfo pps, item 1 won't be invoked, so we
#    need to do the initialization in startup.sh. In this case, $1 is "startup".
# 3. In startup.sh, as early as possible but after final sanity check. The
#    script will put results in the otainfo pps obj. In this case, $1 is "final"
case "${1}" in
	preboot|startup)
		update_ota_pps_preboot
		;;
	final)
		update_ota_pps_final
		;;
	*)
		Error "${0}: unknown argument ${1}"
		echo "Usage: ${0} [preboot|startup|final]"
		exit 1
		;;
esac

