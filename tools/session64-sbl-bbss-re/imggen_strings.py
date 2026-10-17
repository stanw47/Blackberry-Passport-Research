import re, os

def strings(data, minlen=5):
    return re.findall(rb"[\x20-\x7e]{%d,}" % minlen, data)

def show(p, keys, label):
    d = open(p, "rb").read()
    ss = [s.decode() for s in strings(d)]
    print(f"\n===== {label}  ({p}, {len(d)} B) =====")
    for k in keys:
        hits = [s for s in ss if k.lower() in s.lower()]
        print(f"  [{k}] x{len(hits)}")
        for s in hits[:12]:
            print(f"      {s[:100]}")

IMG = r"F:\imggen"
DEV = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\partition-dumps"

keys = ["authboot","permission denied","Flash boot regions","hlos","signature",
        "token","unlock","bootchain","fastboot","yellow","Vol_UP","Vol_DN",
        "Reverting","verified","verification","secure","insecure","bootcontrol"]

show(os.path.join(IMG,"aboot.mbn"), keys, "imggen aboot.mbn (engineering)")
show(os.path.join(DEV,"aboot.img"), keys, "device aboot.img (production)")

# GPT/MCT blobs
for n in ["boot_gpt_insecure.bin","boot_gpt_secure.bin","user_gpt.bin"]:
    p = os.path.join(IMG, n)
    d = open(p,"rb").read()
    print(f"\n===== {n} ({len(d)} B) first 0x120 =====")
    for off in range(0, min(len(d),0x120), 16):
        chunk = d[off:off+16]
        print(f"  {off:04x}  {chunk.hex(' ')}  {''.join(chr(c) if 32<=c<127 else '.' for c in chunk)}")

# perm / nvuser content
for n in ["perm.img","nvuser.img","blog.img","boardid.img","prdid.img"]:
    p = os.path.join(IMG,n); d=open(p,"rb").read()
    ss=[s.decode() for s in strings(d,6)]
    print(f"\n===== {n} ({len(d)} B) distinct-ish strings: {len(ss)} =====")
    for s in ss[:25]: print(f"      {s[:100]}")
    # nonzero ranges
    nz=[i for i,b in enumerate(d) if b]
    print(f"      nonzero bytes: {len(nz)}  first={nz[0] if nz else None} last={nz[-1] if nz else None}")
