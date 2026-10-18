path=r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\tools\session62-bcota-android\pulled\bkup_bc_update"
data=open(path,"rb").read()
for base in range(0x400,0x980,0x40):
    chunk=data[base:base+0x40]
    asc="".join(chr(c) if 32<=c<127 else "." for c in chunk)
    print(f"{base:04x} {asc}")
