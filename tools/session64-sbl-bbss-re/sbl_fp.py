import re, os, struct
bb  = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\vendor\backup_bootchain"
dev = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\partition-dumps"

files = {
 "secure/sbl1r":  os.path.join(bb,"parts_secure","sbl1r.img"),
 "insecure/sbl1r":os.path.join(bb,"parts_insecure","sbl1r.img"),
 "device sbl1":   os.path.join(dev,"sbl1"),
 "device aboot":  os.path.join(dev,"aboot.img"),
}
def hx(b): return b.hex(' ')
for lbl,p in files.items():
    if not os.path.isfile(p): print(lbl,"MISSING"); continue
    d=open(p,"rb").read()
    print(f"\n===== {lbl} ({len(d)} B) header 0x0-0x80 =====")
    for o in range(0,0x80,16):
        c=d[o:o+16]; print(f"  {o:04x}  {hx(c)}  {''.join(chr(x) if 32<=x<127 else '.' for x in c)}")

# locate key strings + offsets
probe = {
 "boot_authenticator.c": None, "SHA256 auth failure": None, "Invalid SBL header": None,
 "Failed to read the SBL header": None, "Authenticating SBL": None, "Jump to SBL": None,
 "SBL1, End": None, "SBL1, Delta": None, "Signal PBL to Jump to RPM FW": None,
 "Processor bound and verified": None, "verifyBind() Failed": None,
 "Failed to find the SBL partition": None, "Loading %s SBL": None, "SBL returned!": None,
 "Backup boot key combo active": None, "Debug board switch forcing backup boot chain": None,
}
for lbl,p in files.items():
    if not os.path.isfile(p): continue
    d=open(p,"rb").read()
    print(f"\n--- {lbl} string offsets ---")
    for s in probe:
        i=d.find(s.encode())
        if i>=0: print(f"  {i:#08x}  {s}")

# magic probes
for lbl,p in files.items():
    if not os.path.isfile(p): continue
    d=open(p,"rb").read()
    for magic in [b"\x84\x4b\xdc\xd1", b"\xd1\xdc\x4b\x84", b"\x7fELF"]:
        i=d.find(magic)
        print(f"  [{lbl}] magic {magic.hex()} @ {i:#x}")
