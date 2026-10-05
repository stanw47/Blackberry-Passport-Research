#!/bin/sh

## Used as inclusion guard
__DEFINED_COMMON_SH__="1"

# required for scripts like wifi,smbd etc.
if [ -z "${__DEFINED_ENV_PERFORMANCE_SH__}" ]; then
   . "${BASEFS}/scripts/env.performance"
fi

###[ COMMON FUNCTIONS ]#####################################################
## Check if fd 3 and 4 are open and open them if they arent.
# open fd 3 and redirect it to stdout
[ -t 3 ] || exec 3>&1
# open fd 4 and redirect it to stderr
[ -t 4 ] || exec 4>&2
############################################################################

SYS_FATAL()
{
    typeset -Z4 ERROR_CODE=$1
    shift
    1>&4 print "    \033[1;30m[ \033[1;31mSystem Fatal\033[0m $* \033[1;30m]\033[0m"
    [[ "$uname_m" != *x86pc* ]] && [ -e "/dev/slog2/error" ] && print "[System Fatal $*]" 1>&2
    [ -e /tmp/wipe.log ] && cat /tmp/wipe.log >&4
    DISP_IMAGE "${BASEFS}/usr/share/errorcodes/bb10-${ERROR_CODE}.png"
    ${BASEFS}/scripts/brickedmux.sh
}

Fatal() {
    1>&4 print "    \033[1;30m[ \033[1;31mFatal\033[0m $* \033[1;30m]\033[0m"
    [[ "$uname_m" != *x86pc* ]] && [ -e "/dev/slog2/error" ] && print "[Fatal $*]" 1>&2
    exit 1
}

Error() {
    1>&4 print "    \033[1;30m[ \033[1;31mError\033[0m $* \033[1;30m]\033[0m"
    [[ "$uname_m" != *x86pc* ]] && [ -e "/dev/slog2/error" ] && print "[Error $*]" 1>&2
}

Warning() {
    1>&4 print "    \033[1;30m[ \033[1;33mWarning\033[0m $* \033[1;30m]\033[0m"
   [[ "$uname_m" != *x86pc* ]] && [ -e "/dev/slog2/error" ] && print "[Warning $*]" 1>&2
}

Info() {
	1>&3 print "    \033[1;30m[ \033[1;36mInfo\033[0m $* \033[1;30m]\033[0m"
    [[ "$uname_m" != *x86pc* ]] && [ -e "/dev/slog2/info" ] && print "[Info $*]"
}

Print() {
    1>&3 print ${1+"$@"}
    [[ "$uname_m" != *x86pc* ]] && [ -e "/dev/slog2/info" ] && print ${1+"$@"}
}

#############################################
#
# Print the line with indentation and header
# The header and message body will be printed
# in different colors on Terminal
#
# $1: the indentation
# $2: header
# $3: body
#
##############################################
PrintHeader() {
    local indent=${1}
    local header=${2}
    local color=${3}
    shift 3
    1>&3 print "${indent}\033[1;30m[ ${color}${header}\033[0m$* \033[1;30m]\033[0m"
    [[ "$uname_m" != *x86pc* ]] && [ -e "/dev/slog2/info" ] && print "${indent}""${header}"${1+"$@"}
}

#Runlevel Prints Indent Level 0
Runlevel_Start() {
    PrintHeader "" "START RUNLEVEL " "\033[1;32m" "$*"
}

Runlevel_Stop() {
    PrintHeader "" "STOP RUNLEVEL " "\033[1;32m" "$*"
}

Runlevel_Shutdown() {
    PrintHeader "" "SHUTDOWN RUNLEVEL " "\033[1;32m" "$*"
}

#Service Prints Indent Level 1
Service_Start() {
    PrintHeader "  " "START SERVICE: " "\033[1;32m" "$*"
}

Service_End() {
    PrintHeader "  " "END SERVICE " "\033[1;32m" "$*"
}

Service_Stop() {
    PrintHeader "  " "STOP SERVICE: " "\033[1;32m" "$*"
}

Service_Shutdown() {
    PrintHeader "  " "SHUTDOWN SERVICE: " "\033[1;32m" "$*"
}

Service_Restart() {
    PrintHeader "  " "RESTART SERVICE: " "\033[1;32m" "$*"
}

Service_Reload() {
    PrintHeader "  " "RELOAD SERVICE: " "\033[1;32m" "$*"
}

#Prints Indent Level 2
Starting() {
    PrintHeader "    " "Starting " "\033[1;32m" "$*"
}

Stopping() {
    PrintHeader "    " "Stopping " "\033[1;32m" "$*"
}

Restarting() {
    PrintHeader "    " "Restarting " "\033[1;32m" "$*"
}

Reloading() {
    PrintHeader "    " "Reloading " "\033[1;32m" "$*"
}

Migrate_Start() {
    PrintHeader "    " "Start Migration " "\033[1;32m" "$*"
}

Migrate_Stop() {
    PrintHeader "    " "Stop Migration " "\033[1;32m" "$*"
}

WAITFOR() {
    if [ -n "${ENABLE_BMETRICS}" ]; then
        QBMETRICS_SNAPSHOT waitfor_${BMETRICS_NAME}_beg_$2;
    fi
    qwaitfor ${2?} ${3:-60} || case "${4}" in
        FATAL) Fatal "${1} failed" ;;
        ERROR) Error "${1} failed" ;;
        *) Warning "${1} failed" ;;
    esac
    if [ -n "${ENABLE_BMETRICS}" ]; then
        QBMETRICS_SNAPSHOT waitfor_${BMETRICS_NAME}_end_$2;
    fi
}

DISP_IMAGE() {
    #if screen is not running then start it (ex. after nuke)
    if [ `${BASEFS}/bin/pidin -p screen -f P | ${BASEFS}/usr/bin/wc -l` -lt 2 ]; then
        ${BASEFS}/scripts/startup.sh screen start
    fi
    CMN_ON -d ${BASEFS}/bin/splash_display -f ${1}
}

DISP_IMAGE_SHUTDOWN() {
    DISP_IMAGE ${1}
    sleep ${2}
    shutdown -S s
}

WAITFOR_ACL() {
    if [ -n "${ENABLE_BMETRICS}" ]; then
        QBMETRICS_SNAPSHOT waitacl_${BMETRICS_NAME}_beg_$2;
    fi
    if [ ! -e  ${2?} ]; then
        Info "Waiting acl on ${2} : ${3:-60}"
        WAITFOR "${2}" "${2}" "${3:-60}"
    fi

    Info "Granting acl ${1} on ${2}"
    ${BASEFS}/bin/setfacl ${1} ${2}
    [ ${?} != 0 ] && Warning "ACL Failure for ${1} on ${2}"
    if [ -n "${ENABLE_BMETRICS}" ]; then
        QBMETRICS_SNAPSHOT waitacl_${BMETRICS_NAME}_end_$2;
    fi
}

ADD_USER_ACL() {
	local user=${1}
	local mode=${2}
	shift 2
	local paths=${*}
	${BASEFS}/bin/setfacl -m user:${user}:${mode} ${paths}
	[ ${?} != 0 ] && Warning "ACL Failure on ${paths}"
}

ADD_GROUP_ACL() {
	local group=${1}
	local mode=${2}
	shift 2
	local paths=${*}
	${BASEFS}/bin/setfacl -m group:${group}:${mode} ${paths}
	[ ${?} != 0 ] && Warning "ACL Failure on ${paths}"
}

# Usage $(NTH_TOKEN "token string" "separator" token_number)
# Token array index starts at 0
NTH_TOKEN() {
    OIFS=$IFS; IFS="${2?}"; set -A ARRAY ${1?}; echo ${ARRAY[${3?}]}; IFS=$OIFS;
}

CMN_ON() {
    qon ${1+"${@}"}
}

CMN_LINK() {
    qln ${1+"${@}"}
}

QBMETRICS() {
    qbmetrics ${1+"${@}"}
}

QBMETRICS_START() {
    QBMETRICS service_start ${1?}
}

QBMETRICS_STOP() {
    QBMETRICS service_stop ${1?}
}

QBMETRICS_SNAPSHOT() {
    QBMETRICS snapshot ${1?}
}

TRIM_SPACE() {
    typeset -L var=${*}
    typeset -R var=${var}
    echo ${var}
}

PPS_VALUE()
{
    #BMETRICS_START "pps_value_${2?}"
    local value=""
    OIFS=${IFS};IFS="
"
    cat ${1?} | {
        while read -r LINE; do
            OIFS2=${IFS}; IFS=":"; set -A ARRAY ${LINE};
            if [ "${2?}" = "${ARRAY[0]}" ]; then
                value=${LINE#*:*:}
                IFS=${OIFS2}
                break
            fi
            IFS=${OIFS2}
        done
        IFS=${OIFS}
        print "${value}"
        #BMETRICS_STOP "pps_value_${2?}"
    }
}

PPS_VALUE_EXISTS()
{
    #BMETRICS_START "pps_value_exists_${2?}"
    local value=""
    OIFS=${IFS};IFS="
"
    cat ${1?} | {
        while read -r LINE; do
            OIFS2=${IFS}; IFS=":"; set -A ARRAY ${LINE};
            if [ "${2?}" = "${ARRAY[0]}" ]; then
                value=${2?}
                IFS=${OIFS2}
                break
            fi
            IFS=${OIFS2}
        done
        IFS=${OIFS}
        print "${value}"
        #BMETRICS_STOP "pps_value_exists_${2?}"
    }
}


PPS_GET_ATTRIB_LINE()
{
       #BMETRICS_START "pps_value_exists_${2?}"
    local value=""
    OIFS=${IFS};IFS="
"
    cat ${1?} | {
        while read -r LINE; do
            OIFS2=${IFS}; IFS=":"; set -A ARRAY ${LINE};
            if [ "${2?}" = "${ARRAY[0]}" ]; then
                value=${LINE}
                IFS=${OIFS2}
                break
            fi
            IFS=${OIFS2}
        done
        IFS=${OIFS}
        print "${value}"
        #BMETRICS_STOP "pps_value_exists_${2?}"
    }
}

cmp_version () {
    if [[ $1 == $2 ]]
    then
        return 0
    fi
    local IFS='.'

    local ver1
    set -A ver1 ${1}

    local ver2
    set -A ver2 ${2}

    # fill empty fields in ver1 with zeros

    i=${#ver1[*]}
    while [[ $i -lt ${#ver2[*]} ]]; do
        ver1[$i]=0
        i=$(expr $i + 1)
    done

    i=0
    while [[ $i -lt ${#ver2[*]} ]]; do
        if [[ -z ${ver2[$i]} ]]
        then
            # fill empty fields in ver2 with zeros
            ver2[i]=0
        fi
        if ((${ver1[$i]} > ${ver2[$i]}))
        then
            return 1
        fi
        if ((${ver1[$i]} < ${ver2[$i]}))
        then
            return 2
        fi
	i=$(expr $i + 1)
    done

    #Versions are equal
    return 0

}

Terminate() {
    local binary=$(basename ${1})
    if [ "1" == "$TerminateFast" ]; then
        CMN_ON -d slay -fQ -sTERM "${binary}"
    else
        typeset -i count=3
        [ -n "${2}" ] && count=${2}
        local check_delay=1
        [ -n "${3}" ] && check_delay=${3}
        slay -fQ -sTERM "${binary}" || slay -fQ -sCONT "${binary}"
        slay -fQ -sNULL "${binary}"
        typeset -i still_alive=$?
        while  ((still_alive > 0 )) && ((count-- > 0)) ; do
            sleep ${check_delay};
            slay -fQ -sNULL "${binary}"
            still_alive=$?
        done
        if ((still_alive > 0)); then
            slay -fQ -sKILL "${binary}"
        fi
    fi
}

#################
#
# Get the value of a PPS attribute.
#
# $1: the attribute (line of PPS object)
#
#################
PPS_ATTRVALUE ()
{
	echo ${1#*:*:}
}

#################
#
# Get a named attribute from a PPS object.
#
# $1: the attribute name to get
# $2: the PPS object to get it from
#
#################
PPS_GETATTR ()
{
	echo $(grep "^${1}:" $2)
}

#################
#
# Get a named attribute's value from a PPS object.
#
# $1: the attribute name to get
# $2: the PPS object to get it from
#
#################
PPS_VALUEOF ()
{
	local attr=$(PPS_GETATTR $1 $2)

	if [ -n "$attr" ]; then
		echo $(PPS_ATTRVALUE "$attr")
	fi
}

#################
#
# Merge attributes from one PPS object if they do not exist in another
#
# $1: the soruce PPS object
# $2: the destination PPS object
# $3: the list of attributes to merge
#################
PPS_MERGE_OBJECT ()
{
	src_obj=$1
	dst_obj=$2
	attribs=$3

	# Check to see if the source and target exist and there is atleast one attirbute to merge
	if [ -e "${src_obj}" ] && [ -e "${dst_obj}" ] && [ "${attribs}" != "" ]; then
		for attrib in ${attribs}; do
			force=${attrib##*:}
			attrib=${attrib%%:*}
			SRC_LINE=$(PPS_GET_ATTRIB_LINE "${src_obj}" "${attrib}")
			[ -z "${SRC_LINE}" ] && continue
			if [ "FORCE" = "${force}" ]; then
				print "${SRC_LINE}" >> ${dst_obj}
			else
				DST_VAL_EXISTS=$(PPS_VALUE_EXISTS  "${dst_obj}" "${attrib}")
				[ -n "${DST_VAL_EXISTS}" ] && continue
				print "${SRC_LINE}" >> ${dst_obj}
			fi
		done
	fi
}

#################
#
# Display's an alert dialog with a message
#
# $1: Dialog Title
# $2: Dialog Text Message
#
# Can't use \n in the message text as it's not parsed properly by shell script below
#################
DISPLAY_SYSTEM_DIALOG()
{
	CMN_ON -d ${BASEFS}/bin/ksh -c "
		. ${BASEFS}/scripts/common.sh
		DISPLAY_SYSTEM_DIALOG_INTERNAL \"${1}\" \"${2}\"
	"
}

DISPLAY_SYSTEM_DIALOG_INTERNAL()
{
	exec 5<> /pps/services/dialog/system-control
	print "msg::show\nid::dlg\ndat:json:{\"className\":\"AlertDialog\",\"desc_obj\":{\"buttons\":[{\"label\":\"Ok\"}],\"titleText\":\""${1}"\",\"messageText\":\""${2}"\",\"groupId\":null}}" >&5

	while :; do
		reply=$(cat <&5)
		if [ -n "${reply}" ]; then
			break
		fi
		sleep 1
	done
	exec 5<&-
}

#################
#
# Check if the device is insecure using persist-tool.
# Print "true" if the device is insecure, otherwise print "false"
#
#################
IS_INSECURE_DEVICE()
{
	local isInsecureDevice="$(/proc/boot/persist-tool -b -sIsInsecureDevice)"
	if [[ "${isInsecureDevice}" == *IsInsecureDevice:\ true* ]]; then
		echo "true"
	else
		echo "false"
	fi
}

#################
#
# This function re-prioritizes a process by matching the process name and arguments and setting that pid to a new priority.
# The first process that matches these critera will be chosen. If the arguments are NULL then we re-prioritize (with slay)
# the first matching process name.
#
# Options:
# -E - Will try to match the arguments exactly
#
# $1: process name
# $2: argument to match
# $3: new priority value (1 - 32)
#
#################

SlayPriorityByMatch()
{
	local OPTIND # Required to be local if using getotps in a function
	local exact_match="false"

	while getopts "R:E" opt; do
		case $opt in
			E) exact_match="true";;
			\?) return 1 ;;
		esac
	done
	shift $(($OPTIND - 1))
	local process=${1}
	local match=${2}
	local priority=${3}
	local pid=-1

	[ -z "${process}" ] && Error "Process cannot be empty" && return 1
	[ -z "${match}" ] && Error "Match cannot be empty" && return 1
	[ -z "${priority}" ] && Error "Priority cannot be empty" && return 1

	# Find the list of processes that match the process name
	pids=$(pidin -p ${process} arg)
	local old_ifs="${IFS}";
	IFS="
"

	for line in ${pids}; do
		#Skip the standard header returned from pidin
		[[ "${line}" == *"pid Arguments"* ]] && continue

		#Determine PID for this line
		local old_ifs2=${IFS}; IFS=" "; set -A split_line ${line} ; local tmp_pid=${split_line[0]}; IFS=${old_ifs2}

		# Do the matching
		if [ "${exact_match}" == "true" ]; then
			# Match exactly the tmp line pid, the process, and the args.
			[[ "${line}" == "${tmp_pid} ${process} ${match}" ]] && pid=${tmp_pid} && break
		else
			# Pattern match the pid, the process, and pattern match on the args.
			[[ "${line}" == *${tmp_pid}*${process}*${match}* ]] && pid=${tmp_pid} && break
		fi
	done
	IFS=${old_ifs}

	# Check that we only have a single number sequnce and nothing else i.e. not "1234 5678"
	# This check will also catch pid set to -1, i.e. could not find it.
	[[ "${pid}" != +([0-9]) ]] && Info "Could not find process" && return 1

	# Reprioritize the process
	Info "Re-prioritizing PID:${pid} to Priority:${priority}"
	slay -P${priority} ${pid}
}

# $1 = file/folder to query
GET_GROUP()
{
	getfacl ${1} | while read cmt type value; do
		if [ "$type" = "group:" ]; then
			# Some group ids have an associated group name that getfacl will return instead
			# of the id. Make sure we always return the id.
			id=$(grep ^${value}: /etc/group | cut -d: -f3)
			if [ "$id" = "" ]; then
				echo $value
			else
				echo $id
			fi
			return
		fi
	done
	return
}

# $1 = file/folder to query
GET_OWNER()
{
	getfacl ${1} | while read cmt type value; do
		if [ "$type" = "owner:" ]; then
			echo $value
			return
		fi
	done
	return
}

