import re, sys
p = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\emmc-full-backup\dumps\aboot.img"
d = open(p, "rb").read()
BASE = 0x100c4000
print(f"aboot.img size {len(d):#x} ({len(d)})")

# 1) confirm the auth strings + offsets
for off in (0x0ab3ac,0x0ab3b8,0x0ab408,0x0ab490,0x0ab568,0x0ab590,0x0ab5d8):
    seg = d[off:off+80].split(b"\x00")[0]
    print(f"  str @{off:#08x}: {seg[:80]!r}")

# 2) find code that references the auth strings (ARM literal pools)
def find_refs(target_va, lo=0x10000, hi=0x90000):
    hits = []
    for i in range(lo, hi-4, 4):
        v = int.from_bytes(d[i:i+4], "little")
        if v == target_va:
            hits.append(i)
    return hits

for name, off in (("auth start",0x0ab568),("auth done",0x0ab590),("triple",0x0ab490),("ANDROID!",0x0ab408)):
    va = BASE + off
    r = find_refs(va)
    print(f"{name} VA={va:#x} literal-pool refs at: " + ", ".join(hex(x) for x in r[:10]))
