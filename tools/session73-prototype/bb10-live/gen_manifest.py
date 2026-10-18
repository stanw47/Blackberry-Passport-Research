import os, hashlib, datetime
D = r"F:\Passport-Originals"
meta = [
 ("orig_boot0.bin",       "BB10 boot chain (SBL1) + MCT/build-info; imggen src_bb0"),
 ("orig_boot1.bin",       "boot1 (blank/backup)"),
 ("orig_user0_128M.bin",  "user0 first 128 MiB: GPT + MCT window (nvram/cal_work/cal_backup) = device identity/cal; imggen src_user0"),
 ("part_nvram0.bin",      "NV partition (runtime NV / identity, binary/BCD)"),
 ("part_cal_work0.bin",   "RF calibration working area (/efs)"),
 ("part_dmi0.bin",        "DMI (device management info)"),
 ("part_uda0.bin",        "UDA (MCT + unique/identity data)"),
 ("part_radio0.bin",      "modem OS/filesystem (128 MiB)"),
]
lines = []
lines.append("Passport (windermereemea / BLACKBERRY-E538) - eMMC originals")
lines.append("Captured %s via rooted (uid 0) SSH: echo '<cmd>' | /base/bin/__root ; /base/bin/dd" % datetime.date.today())
lines.append("Device: QNX 8.0.0 / BB10 10.3.3.3216 / MSM8974AA / WINDERMEREEMEA_Rev:05")
lines.append("")
lines.append("%-24s %12s  %-64s  %s" % ("file","size","sha256","what it holds"))
for f, note in meta:
    p = os.path.join(D, f)
    if not os.path.exists(p):
        lines.append("%-24s %12s  %-64s  %s" % (f, "MISSING", "", note)); continue
    d = open(p, "rb").read()
    lines.append("%-24s %12d  %-64s  %s" % (f, len(d), hashlib.sha256(d).hexdigest(), note))
lines.append("")
lines.append("NOT captured / limits:")
lines.append("  - rpmb0: dd -> 'Inappropriate I/O control operation' (RPMB needs the auth/key path)")
lines.append("  - mod_nvram -b: EPERM in this shell (BB10 native NV backup not produced)")
lines.append("  - os0/os1 (OS A/B) and full user0 (28 GB) not dumped (OS is board-generic)")
lines.append("")
lines.append("IMEI: stored binary/BCD in the NV/EFS (not ASCII). The conversion-preserved copy is the")
lines.append("MCT window inside orig_user0_128M.bin (cal_work/cal_backup/nvram); live copies are")
lines.append("part_nvram0.bin + part_cal_work0.bin + part_uda0.bin. Restore all of these to preserve identity.")
txt = "\n".join(lines) + "\n"
open(os.path.join(D, "MANIFEST.txt"), "w", encoding="utf-8").write(txt)
print(txt)
