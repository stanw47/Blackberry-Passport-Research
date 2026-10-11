#!/usr/bin/env python3
import struct, sys

_BUI = "<II4sI64s16s16s12sIIIIIIIIIIIIIIIIIIIIII40s1024s"

def parse_build_info(b):
    f = struct.unpack(_BUI, b)
    assert f[1] == 0x4fc, "bad build info size %#x" % f[1]
    ver = ".".join(str(x) for x in f[2])
    return {
        'version': ver,
        'hwid': f[3],
        'builder': f[5].split(b'\0',1)[0].decode(),
        'date': f[6].split(b'\0',1)[0].decode() + ' ' + f[7].split(b'\0',1)[0].decode(),
        'secure': f[8] == 0,
        'rev_table_offset': f[18] - f[15]*4 - f[13]*2,
        'rev_table_size': f[13]*2,
        'mct_offset': f[18] - f[15]*4,
        'mct_size': f[15]*4,
        'processor_id': f[20],
        'usbloader_id': f[24],
    }

def parse_rev_table(t):
    f = struct.unpack("<"+"B"*len(t), t)
    ret = {}
    for i in range(len(t)//2):
        k, v = f[i*2], f[i*2+1]
        if k == 0xff: continue
        if k == 1: k = 'pcb_rev'
        if k == 4: k = 'pop_rev'
        ret[k] = v
    return ret

def parse_mct(mct):
    magic = struct.unpack('<I', mct[:4])[0]
    assert magic == 0x92be564a, "bad MCT magic %#x" % magic
    minor, major = struct.unpack('<HH', mct[4:8])
    assert major == 1, "bad MCT major %d" % major
    mct = mct[8:]
    parts = {}
    while len(mct) > 0:
        _type, size = struct.unpack('<BB', mct[:2])
        if _type == 9 or _type == 0xff: break
        if size > len(mct): raise Exception('bad MCT entry')
        if _type == 0x39:
            unk, flags, name, start, end = struct.unpack('<BB12sII', mct[2:size])
            name = name.split(b'\0',1)[0].decode()
            if name not in parts:
                parts[name] = {'offset': start<<16, 'size': ((end+1)<<16)-(start<<16)}
        mct = mct[size:]
    return parts

def main(path):
    d = open(path,'rb').read()
    idx = d.find(b"RIM BlackBerry Device")
    assert idx > 0, "no build info string"
    off = idx - 0x10
    bi = parse_build_info(d[off:off+0x4fc])
    print("=== BUILD INFO ===")
    for k,v in bi.items(): print(" %-18s %s" % (k, hex(v) if isinstance(v,int) else v))
    rev = parse_rev_table(d[bi['rev_table_offset']:bi['rev_table_offset']+bi['rev_table_size']])
    print("=== REV TABLE ===", rev)
    mct = parse_mct(d[bi['mct_offset']:bi['mct_offset']+bi['mct_size']])
    print("=== MCT PARTITIONS (offset in user0-abs eMMC, size) ===")
    for name in sorted(mct, key=lambda n: mct[n]['offset']):
        p = mct[name]
        print(" %-12s off=%#012x size=%#010x end=%#012x" % (name, p['offset'], p['size'], p['offset']+p['size']))
    if 'cal_backup' in mct:
        p = mct['cal_backup']
        need = p['offset'] + p['size'] - 0x200000
        print("=== NEEDED user.img size >= %#x (%d bytes, %.1f MiB) ===" % (need, need, need/1048576))

if __name__ == "__main__":
    main(sys.argv[1])
