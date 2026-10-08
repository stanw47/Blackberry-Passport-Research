#!/usr/bin/env python3
"""QCFM (.signed) packer — port of bb10mt qcfm.pas: _packMFCQ (ver=2, fast mode).

Layout (verified against balika v2.0-android.signed):
  @0x00  mhf1 (32B): 'mfcq', checksum=0, version=1, nheaders=0,
                     headersz = mhf2.headersz + 0x20 (=data offset),
                     datachecksum=0, flags=mhf2 offset (0x20), dummy=0
  @0x20  mhf2 (28B): 'mfcq', header_cksum=0, version=0x20000, length=28,
                     nheaders=N, headersz = 0x3C + 60*N, data_crc=0
  @0x3C  N x { cf2 (44B): 'pfcq', version=0x20000, length=60, _type,
               rrecOffset=44, nrecords=1, hwvOffset=0, hwvNum=0, sigOffset=0,
               sigSize=0, blocksize=0x10000
             , rr2 (16B): 'rrcq', length=16, offset=0, count=ceil(size/bs) }
  [zeros to mhf1.headersz = 0x5C + 60*N]
  data blocks (each file zero-padded to a whole block)
"""
import struct, sys, os

BS = 0x10000
EXT2TYPE = {'.nvram':3,'.ufs':5,'.mbr':6,'.sig':7,'.ifs':8,'.rcfs':9,
            '.radio.mbr':0x0A,'.radio.sig':0x0B,'.radio.rcfs':0x0C,
            '.calwork':0x0F,'.calbackup':0x10,'.dmi':0x11,'.dmi.mbr':0x12,
            '.dmi.fsys':0x13,'.os':0x18,'.sig2':0x89,'.radio.sig2':0x8C,
            '.dmi.sig2':0x93}

def ftype(name):
    base = name.lower()
    for ext, t in sorted(EXT2TYPE.items(), key=lambda kv: -len(kv[0])):
        if base.endswith(ext):
            return t
    raise SystemExit("unknown record extension: %s" % name)

def pack(files, out):
    n = len(files)
    mhf2_hdr = 0x3C + 60 * n          # mhf2 + control files + run records
    data_off = mhf2_hdr + 0x20        # mhf1.headersz
    sizes = [os.path.getsize(f) for f in files]
    mhf1 = struct.pack('<4sIIIIII4s', b'mfcq', 0, 1, 0, data_off, 0, 0x20, b'\0\0\0\0')
    mhf2 = struct.pack('<4sIIIIII', b'mfcq', 0, 0x20000, 28, n, mhf2_hdr, 0)
    body = bytearray()
    for f, sz in zip(files, sizes):
        t = ftype(f)
        cnt = (sz + BS - 1) // BS
        body += struct.pack('<4sIIIIIIIIII', b'pfcq', 0x20000, 60, t, 44, 1,
                            0, 0, 0, 0, BS)
        body += struct.pack('<4sIII', b'rrcq', 16, 0, cnt)
    assert len(body) == 60 * n
    pad = b'\0' * (data_off - 0x3C - len(body))
    with open(out, 'wb') as w:
        w.write(mhf1); w.write(mhf2); w.write(body); w.write(pad)
        for f, sz in zip(files, sizes):
            left = sz
            with open(f, 'rb') as r:
                while left > 0:
                    b = r.read(min(BS, left))
                    if not b:
                        raise SystemExit("short read: %s" % f)
                    w.write(b); left -= len(b)
                    if left == 0 and len(b) < BS:
                        w.write(b'\0' * (BS - len(b)))

if __name__ == '__main__':
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    pack(sys.argv[2:], sys.argv[1])
    print("wrote %s (%d bytes)" % (sys.argv[1], os.path.getsize(sys.argv[1])))
