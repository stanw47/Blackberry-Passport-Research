import struct, os, re
bb  = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\vendor\backup_bootchain"
p = os.path.join(bb,"parts_secure","sbl1r.img")
d = open(p,"rb").read()
base = 0xf800c000
size = len(d)

# string map
strs = {}
for m in re.finditer(rb"[\x20-\x7e]{4,}", d):
    strs[m.start()] = m.group().decode()

def nearest_str(off):
    for k in sorted(strs):
        if k <= off < k+len(strs[k])+8:
            return strs[k]
    return None

# find literal words that are pointers into [base, base+size)
hits = {}
for o in range(0, len(d)-4, 2):        # 2-byte step (ARM/Thumb pools)
    w = struct.unpack_from("<I", d, o)[0]
    if base <= w < base+size:
        fo = w - base
        if fo == 0: continue
        hits.setdefault(fo, []).append(o)

print(f"sbl1r: {len(hits)} distinct internal pointer targets")
# report targets that resolve to a string
named = []
for fo in sorted(hits):
    s = nearest_str(fo)
    if s:
        named.append((fo, s, hits[fo][:4]))
print(f"\n== pointer targets landing on/near a string ({len(named)}) ==")
for fo, s, srcs in named:
    print(f"  ->{fo:#07x}  {s[:70]!r}   from {[hex(x) for x in srcs]}")

# how many pointers point into the code region
print(f"\ntotal pointer occurrences: {sum(len(v) for v in hits.values())}")
