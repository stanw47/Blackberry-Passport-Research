import hashlib, os, re

def h(p):
    x = hashlib.sha256(); 
    with open(p,"rb") as f:
        for b in iter(lambda: f.read(1<<20), b""): x.update(b)
    return x.hexdigest()

root = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research"
bb = os.path.join(root, r"recon\passport-android-dump\vendor\backup_bootchain")
img = r"F:\imggen"
dev = os.path.join(root, r"recon\passport-android-dump\partition-dumps")

def strcount(p, keys):
    d = open(p,"rb").read()
    ss = [s.decode() for s in re.findall(rb"[\x20-\x7e]{5,}", d)]
    return {k: sum(1 for s in ss if k.lower() in s.lower()) for k in keys}

keys = ["authboot","permission denied","hlos","signature.tkn","bootcontrolblock","vol_up","yellow"]

print("=== parts_insecure ===")
inc = {}
for n in sorted(os.listdir(os.path.join(bb,"parts_insecure"))):
    p=os.path.join(bb,"parts_insecure",n); s=os.path.getsize(p); hh=h(p)
    inc[n]=hh
    print(f"  {s:>9} {hh[:16]} {n}")

print("\n=== parts_secure (compare to insecure) ===")
for n in sorted(os.listdir(os.path.join(bb,"parts_secure"))):
    p=os.path.join(bb,"parts_secure",n)
    if os.path.isfile(p):
        hh=h(p); same = inc.get(n)==hh
        print(f"  {os.path.getsize(p):>9} {hh[:16]} {n}  {'SAME as insecure' if same else 'DIFF'}")

print("\n=== imggen vs insecure-abootr ===")
a = os.path.join(bb,"parts_insecure","abootr.img")
print("  imggen aboot.mbn     ", h(os.path.join(img,"aboot.mbn")))
print("  insecure abootr.img  ", h(a))
print("  imggen bbss.mbn      ", h(os.path.join(img,"bbss.mbn")))
print("  insecure bbss.img    ", h(os.path.join(bb,"parts_insecure","bbss.img")))

print("\n=== keyword counts ===")
for label, p in [("imggen aboot.mbn", os.path.join(img,"aboot.mbn")),
                 ("insecure abootr.img", a),
                 ("secure abootr.img", os.path.join(bb,"parts_secure","abootr.img")),
                 ("device aboot.img", os.path.join(dev,"aboot.img"))]:
    print(f"  {label:<22}", strcount(p, keys))
