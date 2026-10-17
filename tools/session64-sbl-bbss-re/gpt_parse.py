import struct, os
files = {
 "boot_gpt_insecure": r"F:\imggen\boot_gpt_insecure.bin",
 "boot_gpt_secure":   r"F:\imggen\boot_gpt_secure.bin",
 "user_gpt":          r"F:\imggen\user_gpt.bin",
}
def guid(b):
    a,b2,c,d,e = struct.unpack('<IHH', b[0:8]) + (b[8:10], b[10:16])
    d0 = struct.unpack('>H', b[8:10])[0]; d1=struct.unpack('>H',b[10:12])[0]
    return f"{a:08x}-{b2:04x}-{c:04x}-{d0:04x}-{e.hex().upper()}"
for name,path in files.items():
    print(f"\n========== {name} ({path}) ==========")
    if not os.path.exists(path): print("  MISSING"); continue
    d=open(path,'rb').read()
    print(f"  size {len(d)}  first16={d[:16].hex(' ')}")
    off=d.find(b'EFI PART')
    print(f"  'EFI PART' at {off:#x}")
    if off<0: continue
    ver,hdrsz,crc,rsv = struct.unpack_from('<IIII',d,off+8)
    cur,back = struct.unpack_from('<QQ',d,off+24)
    first,last = struct.unpack_from('<QQ',d,off+40)
    dguid = guid(d[off+56:off+72])
    pelba,num,esz,ecrc = struct.unpack_from('<QIII',d,off+72)
    print(f"  ver={ver} hdrsz={hdrsz} cur_lba={cur} backup_lba={back} first_usable={first} last_usable={last}")
    print(f"  disk_guid={dguid}  entries_lba={pelba}(abs {off-512+pelba*512:#x}) num={num} esz={esz}")
    base = off - 512 + pelba*512
    for i in range(num):
        e=base+i*esz
        if e+esz>len(d): break
        tg=guid(d[e:e+16])
        if tg=="00000000-0000-0000-0000-000000000000": continue
        f,s=struct.unpack_from('<QQ',d,e+32)
        nm=d[e+56:e+56+72].decode('utf-16-le','ignore').rstrip('\x00')
        print(f"    [{i}] {nm!r:26} type={tg} lba {f}-{s} ({s-f+1} sec = {(s-f+1)*512} B)")
