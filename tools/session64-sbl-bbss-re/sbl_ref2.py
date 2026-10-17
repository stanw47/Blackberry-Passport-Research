import struct, os, re
bb  = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\vendor\backup_bootchain"
d = open(os.path.join(bb,"parts_secure","sbl1r.img"),"rb").read()
base = 0xf800c000
def refs_for(t):
    out=[]
    for m in re.finditer(re.escape(t.encode()), d):
        fo=m.start(); v=base+fo
        b=struct.pack("<I", v)
        rr=[x.start() for x in re.finditer(re.escape(b), d)]
        if rr: out.append((fo, v, rr))
    return out

for t in ["boot_authenticator.c","SHA256 auth failure","Processor bound and verified",
          "bbry_is_insecure","Insecure fusemap","Secure fusemap",
          "APPSBL Image Loaded, Delta","Failed getting bind status","BBSS"]:
    print(f"\n== {t!r} ==")
    got = refs_for(t)
    if not got: print("   no literal refs (any occurrence)")
    for fo,v,rr in got:
        print(f"   string@fo={fo:#x} va={v:#010x}  literals@ {[hex(x) for x in rr[:6]]}")
