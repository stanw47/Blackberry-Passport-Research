import os, glob, re
sets = {
 "PROTO": [r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\emmc-full-backup\dumps",
           r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\partition-dumps"],
 "RETAIL": [r"F:\Passport-Originals"],
}
pats = [b"032GE4",b"THGBM",b"KLMCG",b"KLMBG",b"KMQG",b"SDIN",b"MTFC",b"Numonyx",b"Micron",b"Hynix",
        b"Toshiba",b"TOSHIBA",b"Samsung",b"SAMSUNG",b"SanDisk",b"Kioxia",b"ISSI",b"Alliance",
        b"FORESEE",b"Longsys",b"Biwin",b"YMTC",b"Kingston",b"Phison",b"stled",b"STLED",b"wled",b"WLED",
        b"backlight",b"Backlight",b"mmc0:0001",b"mmc1:0001"]
for name, dirs in sets.items():
    print("################", name)
    for d in dirs:
        if not os.path.isdir(d): continue
        for f in sorted(glob.glob(os.path.join(d,"*"))):
            if not os.path.isfile(f) or os.path.getsize(f) > 200*1024*1024: continue
            data = open(f,"rb").read()
            hits = {p.decode(): data.count(p) for p in pats if data.count(p)}
            if hits: print("  %-34s %s" % (os.path.basename(f), hits))
