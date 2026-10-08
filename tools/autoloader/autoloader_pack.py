#!/usr/bin/env python3
"""BB10 autoloader (.exe) packer — BlackBerry format, verified byte-level.

Layout (validated against Passport root_v2.exe and Passport_Android_balika_v2.exe):
  [cap.exe bytes up to PE_end]      P = end of last PE section
  u32 0x97C5D59C x3
  80 x 0x00
  u32 count
  count x { u32 0, u32 offset }     offset = P + 0x84 + cumulative size (LE u32)
  zero padding to P + 0x84
  file data, sequentially

Usage: autoloader_pack.py <cap.exe> <out.exe> <input1> [input2 ...]   (max 4)
"""
import struct, sys, os

SIG  = 0x97C5D59C
HDR  = 0x84   # fixed header size from PE end to first file data

def pe_end(path):
    with open(path, 'rb') as f:
        hdr = f.read(0x1000)
        e = struct.unpack_from('<I', hdr, 0x3c)[0]
        if hdr[e:e+4] != b'PE\x00\x00':
            raise SystemExit("not a PE file: %s" % path)
        nsec = struct.unpack_from('<H', hdr, e+6)[0]
        opt  = struct.unpack_from('<H', hdr, e+20)[0]
        sec  = e + 24 + opt
        end = 0
        for i in range(nsec):
            s = hdr[sec+i*40:sec+i*40+40]
            rs, rp = struct.unpack_from('<II', s, 16)
            end = max(end, rp + rs)
        return end

def pack(cap, files, out):
    if len(files) > 4:
        raise SystemExit("too many input files (format allows 4)")
    P = pe_end(cap)
    sizes = [os.path.getsize(x) for x in files]
    hdr = bytearray()
    hdr += struct.pack('<III', SIG, SIG, SIG)
    hdr += b'\x00' * 80
    hdr += struct.pack('<I', len(files))
    off = P + HDR
    for s in sizes:
        hdr += struct.pack('<II', 0, off)
        off += s
    if len(hdr) > HDR:
        raise SystemExit("header overflow")
    hdr += b'\x00' * (HDR - len(hdr))
    with open(out, 'wb') as w:
        with open(cap, 'rb') as f:
            w.write(f.read(P))
        w.write(hdr)
        for x in files:
            with open(x, 'rb') as f:
                while True:
                    b = f.read(1 << 20)
                    if not b:
                        break
                    w.write(b)

if __name__ == '__main__':
    if len(sys.argv) < 4:
        raise SystemExit(__doc__)
    cap, out = sys.argv[1], sys.argv[2]
    pack(cap, sys.argv[3:], out)
    print("wrote %s (%d bytes)" % (out, os.path.getsize(out)))
