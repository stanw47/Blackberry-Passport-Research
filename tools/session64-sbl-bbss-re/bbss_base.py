import re, struct
p = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\vendor\backup_bootchain\parts_secure\bbss.img"
d = open(p, "rb").read()
def score(base):
    n=0; ex=[]
    for o in range(0,len(d)-3,4):
        w=struct.unpack_from('<I',d,o)[0]
        if base<=w<base+len(d):
            m=re.match(rb'[\x20-\x7e]{4,}', d[w-base:w-base+48])
            if m:
                n+=1; ex.append((o,w-base,m.group().decode()[:40]))
    return n,ex
for base in [0xf8050000,0xf805fc00,0xf8040000,0xf800c000,0xf8000000,0xf8060000]:
    n,ex=score(base)
    print(f"base {base:#010x}: {n} string-ptr refs; sample {ex[:3]}")
