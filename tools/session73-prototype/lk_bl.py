p = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\emmc-full-backup\dumps\aboot.img"
d = open(p, "rb").read()
BASE = 0x100c4000
def bl_targets(lo, hi, want):
    hits=[]
    for i in range(lo, hi-4, 4):
        w = int.from_bytes(d[i:i+4],"little")
        if (w & 0x0F000000) != 0x0B000000:   # BL/BLX
            continue
        if (w & 0xFE000000) == 0xFA000000:   # BLX (bit24 set) - skip? BL is cond!=0xF
            pass
        imm24 = w & 0xFFFFFF
        if imm24 & 0x800000:
            imm24 -= 0x1000000
        tgt = (BASE + i) + 8 + (imm24<<2)
        if tgt == want:
            hits.append(i)
    return hits
for want in (0x100d5ea0, 0x100d5e00, 0x100d5f00, 0x1014c880, 0x1014e2d4):
    h = bl_targets(0, len(d), want)
    print(f"BL -> {want:#x} : " + ", ".join(hex(x) for x in h[:20]))
    for x in h[:4]:
        print(f"    ctx @{x:#x}: " + d[x-24:x+24].hex())
