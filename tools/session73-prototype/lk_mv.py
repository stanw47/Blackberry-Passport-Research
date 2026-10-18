p = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\emmc-full-backup\dumps\aboot.img"
d = open(p, "rb").read()
BASE = 0x100c4000
targets = {"auth_start(0xab568)":0x568, "auth_done(0xab590)":0x590, "triple(0xab490)":0x490,
           "postauth(0xab5d8)":0x5d8, "str430(0xab430)":0x430, "str3f0(0xab3f0)":0x3f0}
def scan(imm12):
    res=[]
    for i in range(0, len(d)-8, 4):
        w = int.from_bytes(d[i:i+4],"little")
        if (w & 0xFFF00000)==0xE3000000 and ((w>>4)&0xFFF)==imm12 and ((w>>20)&0xF)==0:
            rd=(w>>12)&0xF
            # look for movt rd in next 40 bytes
            for j in range(i+4, i+48, 4):
                w2=int.from_bytes(d[j:j+4],"little")
                if (w2 & 0xFFF00000)==0xE3400000 and ((w2>>12)&0xF)==rd:
                    imm4=(w2>>16)&0xF; imm12b=(w2>>4)&0xFFF
                    hi=(imm4<<12)|imm12b
                    res.append((i, rd, hi))
                    break
    return res
for name,imm in targets.items():
    r=scan(imm)
    print(f"{name}: " + ", ".join(f"movw@{i:#x} r{rd} hi=0x{hi:04x} -> VA 0x{hi:04x}{imm:03x}" for i,rd,hi in r[:12]))
