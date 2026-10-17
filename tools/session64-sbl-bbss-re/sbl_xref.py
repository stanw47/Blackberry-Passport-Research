import struct, os, re
from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM, CS_MODE_THUMB
bb  = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\vendor\backup_bootchain"
d = open(os.path.join(bb,"parts_secure","sbl1r.img"),"rb").read()
base = 0xf800c000
def va(fo): return base+fo
def fo(v):  return v-base

targets = ["bbry_is_insecure","Insecure fusemap","Secure fusemap",
           "APPSBL Image Loaded, Delta","SHA256 auth failure","Processor bound and verified",
           "Authenticating","boot_authenticator.c","Invalid SBL header",
           "Failed to read the SBL header","Failed getting bind status",
           "BBSS","bsi_","bbss_insecure"]

loc = {}
for t in targets:
    i = d.find(t.encode())
    loc[t] = i
    print(f"  {i:#08x} (va {va(i):#010x})  {t}")

print("\n=== literal refs to each string VA (4-byte LE) ===")
for t,i in loc.items():
    if i<0: continue
    v = va(i)
    b = struct.pack("<I", v)
    refs = [m.start() for m in re.finditer(re.escape(b), d)]
    print(f"  {t!r} -> refs at {[hex(r) for r in refs[:8]]}")

# disassemble ARM windows around the interesting refs
md = Cs(CS_ARCH_ARM, CS_MODE_ARM)
for t in ["APPSBL Image Loaded, Delta","SHA256 auth failure","boot_authenticator.c","Processor bound and verified"]:
    i=loc.get(t,-1)
    if i<0: continue
    v=va(i); b=struct.pack("<I", v)
    for r in [m.start() for m in re.finditer(re.escape(b), d)][:3]:
        lo=max(0,r-0x60); 
        print(f"\n--- code around ref to {t!r} @ {r:#x} (va {va(r):#010x}) ---")
        for ins in md.disasm(d[lo:r+0x10], va(lo)):
            print(f"   {ins.address:#010x}: {ins.mnemonic:<8} {ins.op_str}")
