import struct
dev=r'C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\partition-dumps\aboot.img'
d=open(dev,'rb').read()
isz=struct.unpack_from('<I',d,0x10)[0]
print(f'device aboot image_size={isz:#x} file={len(d):#x}')
shown=0
for o in range(isz, len(d), 16):
    chunk=d[o:o+16]
    if any(chunk):
        print('  {:08x}: {}'.format(o, chunk.hex(' ')))
        shown+=1
        if shown>=6: break
print()
kit=open(r'F:\imggen\aboot.mbn','rb').read()
isz2=struct.unpack_from('<I',kit,0x10)[0]
print(f'kit aboot image_size={isz2:#x} file={len(kit):#x} trailing={len(kit)-isz2} bytes')
print('  trailing:', kit[isz2:].hex(' '))
print()
print('kit  sig_ptr/size @0x18/0x1c:', struct.unpack_from('<II',kit,0x18))
print('kit  cert_ptr/size @0x20/0x24:', struct.unpack_from('<II',kit,0x20))
print('dev  sig_ptr/size @0x18/0x1c:', struct.unpack_from('<II',d,0x18))
print('dev  cert_ptr/size @0x20/0x24:', struct.unpack_from('<II',d,0x20))
