import re
p = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\partition-dumps\aboot.img"
d = open(p, "rb").read()
for lo,hi,t in [(0xab000,0xab600,"unlock/debug/token"),(0xabe00,0xac200,"fastboot"),
                (0xb1ee0,0xb1f60,"verified"),(0xa9600,0xa9640,"unlock(1)"),
                (0xb5940,0xb5980,"unlock(3)"),(0xac140,0xac180,"getvar")]:
    print(f"===== {t}  {lo:#x}-{hi:#x} =====")
    for m in re.finditer(rb'[\x20-\x7e]{4,}', d[lo:hi]):
        print(f"  {lo+m.start():#08x}  {m.group().decode()[:100]}")
