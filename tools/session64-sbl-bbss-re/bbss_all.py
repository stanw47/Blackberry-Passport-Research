import os, re, struct
bb = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\vendor\backup_bootchain"
for name in ["parts_secure\\bbss.img", "parts_insecure\\bbss.img"]:
    p = os.path.join(bb, name)
    d = open(p, "rb").read()
    print(f"\n########## {name}  ({len(d)} bytes, sha256...) ##########")
    print("first 64:", d[:64].hex(' '))
    print("--- strings (>=5 printable, unique-ish) ---")
    seen=set()
    for m in re.finditer(rb'[\x20-\x7e]{5,}', d):
        s=m.group().decode()
        if s not in seen:
            seen.add(s)
            print(f"  {m.start():#07x}  {s[:96]}")
