import re
p = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\partition-dumps\aboot.img"
d = open(p, "rb").read()
print(f"size {len(d)}")
lo,hi = 0xb0000, 0xb9000
print(f"--- strings in {lo:#x}-{hi:#x} ---")
for m in re.finditer(rb'[\x20-\x7e]{4,}', d[lo:hi]):
    print(f"  {lo+m.start():#08x}  {m.group().decode()[:100]}")
