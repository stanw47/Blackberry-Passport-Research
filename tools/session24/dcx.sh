#!/bin/bash
# dcx.sh - run a command as uid 1301 (ddt) via the diagnostics ExecTask channel
# usage: dcx.sh 'shell command with spaces'
export PATH="$HOME/bin/platform-tools:$PATH"
inner="$1"
tok="${inner// /'${IFS}'}"
param="/system/bin/sh -c ${tok}"
printf -v cmd "service call diagnostics 3 i32 1 i32 0 i32 11 s16 '%s'" "$param"
adb shell "$cmd" >/dev/null 2>&1
