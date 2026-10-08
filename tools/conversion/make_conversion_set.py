#!/usr/bin/env python3
"""Build the Passport conversion image set (Balika-faithful, kit aboot).

Why: the prebuilt imggen embeds a *different* engineering aboot (1.95 MB) than
the GitHub kit / balika package (342 KB, md5 afda7cdf...). This rebuilds the
Balika-faithful set for the target board (the board data in the imggen outputs
— nvram/cal/HWI — is preserved).

Inputs:
  --imggen-dir DIR  prebuilt imggen outputs for the board:
                    new_boot1.img (secure boot image), new_user_gpt.bin
  --kit-dir DIR     GitHub imggen files/ (aboot.mbn, sbl1r.mbn, bbss.mbn,
                    stage1-3.mbn, tz.mbn, rpm.mbn, sdi.mbn,
                    boot_gpt_insecure.bin)
  --out-dir DIR     output directory

Outputs:
  boot_secure.img    secure boot GPT + kit components (kit aboot in abootr)
  boot_insecure.img  7-part insecure GPT + kit components + board HWI
  user.img           user-area prefix with the kit aboot in the aboot partition
"""
import argparse, os, struct

def gpt_parts(path):
    d = open(path, 'rb'); d.seek(512); hdr = d.read(92)
    if hdr[:8] != b'EFI PART':
        raise SystemExit("no GPT in %s" % path)
    ptlba = struct.unpack_from('<Q', hdr, 72)[0]
    nent  = struct.unpack_from('<I', hdr, 80)[0]
    esz   = struct.unpack_from('<I', hdr, 84)[0]
    d.seek(ptlba * 512); ents = d.read(nent * esz)
    parts = {}
    for i in range(nent):
        e = ents[i*esz:(i+1)*esz]
        if e[:16] == b'\x00' * 16:
            continue
        name = e[56:128].decode('utf-16le').rstrip('\x00')
        first = struct.unpack_from('<Q', e, 32)[0]
        last  = struct.unpack_from('<Q', e, 40)[0]
        parts[name] = (first * 512, (last - first + 1) * 512)
    return parts

def write_at(path, off, size, data):
    d = bytearray(open(path, 'rb').read())
    if len(data) > size:
        raise SystemExit("component too big at %#x" % off)
    if len(d) < off + size:                     # extend past EOF with zeros
        d.extend(b'\x00' * (off + size - len(d)))
    d[off:off+size] = data + b'\x00' * (size - len(data))
    open(path, 'wb').write(d)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--imggen-dir', required=True)
    ap.add_argument('--kit-dir', required=True)
    ap.add_argument('--out-dir', required=True)
    a = ap.parse_args()
    os.makedirs(a.out_dir, exist_ok=True)
    kit = lambda f: open(os.path.join(a.kit_dir, f), 'rb').read()
    aboot = kit('aboot.mbn')

    # 1. secure boot image: imggen output, abootr <- kit aboot
    src = os.path.join(a.imggen_dir, 'new_boot1.img')
    dst = os.path.join(a.out_dir, 'boot_secure.img')
    open(dst, 'wb').write(open(src, 'rb').read())
    off, sz = gpt_parts(dst)['abootr']; write_at(dst, off, sz, aboot)

    # 2. insecure boot image: kit template + kit components
    dst = os.path.join(a.out_dir, 'boot_insecure.img')
    open(dst, 'wb').write(kit('boot_gpt_insecure.bin'))
    parts = gpt_parts(dst)
    comp = {'bbss':'bbss.mbn','sbl1r':'sbl1r.mbn','tzr':'tz.mbn','rpmr':'rpm.mbn',
            'sdir':'sdi.mbn','abootr':'aboot.mbn'}
    for p, f in comp.items():
        if p in parts:
            off, sz = parts[p]; write_at(dst, off, sz, kit(f))
    # board HWI from the secure image
    sec = open(os.path.join(a.out_dir, 'boot_secure.img'), 'rb').read()
    hoff, hsz = gpt_parts(dst)['hwi']
    soff = gpt_parts(os.path.join(a.out_dir, 'boot_secure.img'))['hwi'][0]
    write_at(dst, hoff, hsz, sec[soff:soff+hsz])

    # 3. user image: imggen output, aboot <- kit aboot
    src = os.path.join(a.imggen_dir, 'new_user_gpt.bin')
    dst = os.path.join(a.out_dir, 'user.img')
    open(dst, 'wb').write(open(src, 'rb').read())
    off, sz = gpt_parts(dst)['aboot']; write_at(dst, off, sz, aboot)

    print("done ->", a.out_dir)

if __name__ == '__main__':
    main()
