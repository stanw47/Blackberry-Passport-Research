import hashlib, os, json

IMG = r"F:\imggen"
DEV = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\partition-dumps"

def h(p):
    x = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            x.update(b)
    return x.hexdigest()

def ls(d):
    out = {}
    for n in os.listdir(d):
        p = os.path.join(d, n)
        if os.path.isfile(p):
            out[n] = (os.path.getsize(p), h(p))
    return out

im = ls(IMG); dv = ls(DEV)
print("===== imggen/files =====")
for n in sorted(im): print(f"  {im[n][0]:>10}  {im[n][1][:16]}  {n}")

devmap = {}
for n, (s, hh) in dv.items():
    devmap.setdefault(hh, []).append(f"{n}({s})")

print("\n===== imggen file -> matching device dump =====")
for n in sorted(im):
    s, hh = im[n]
    m = devmap.get(hh, [])
    print(f"  {n:<28} {'MATCH '+str(m) if m else 'no match'}")

print("\n===== device boot-chain dumps of interest =====")
for n in sorted(dv):
    if any(k in n.lower() for k in ["aboot","sbl1","tz","rpm","sdi","bbss","stage","bcota","boardid","prdid","blog","perm","nvuser"]):
        print(f"  {dv[n][0]:>10}  {dv[n][1][:16]}  {n}")
