import os, re, struct
from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM, CS_MODE_THUMB
from capstone.arm import ARM_OP_REG, ARM_OP_IMM, ARM_REG_PC
p = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\vendor\backup_bootchain\parts_secure\sbl1r.img"
d = open(p,"rb").read()
base = 0xf800c000
targets = {base+o: s.decode() for o,s in []}
want = ["Image Load, Start","SHA256 auth failure","APPSBL Image Loaded","boot_config.c",
        "bbry_is_insecure","Insecure fusemap","Secure fusemap","boot_authenticator.c"]
tv = {}
for w in want:
    for m in re.finditer(re.escape(w.encode()), d): tv[base+m.start()]=w

def str_at(t):
    if base<=t<base+len(d):
        m=re.match(rb"[\x20-\x7e]{4,}", d[t-base:t-base+64])
        if m: return m.group().decode()
    return None

def scan(mode):
    md=Cs(CS_ARCH_ARM, mode); md.detail=True
    out=[]
    step = 2 if mode==CS_MODE_THUMB else 4
    for off in range(0x40, len(d)-4, step):
        for ins in md.disasm(d[off:off+4], base+off):
            ops=ins.operands
            if not ops: continue
            if not any(o.type==ARM_OP_REG and o.reg==ARM_REG_PC for o in ops): continue
            imm=None
            for o in ops:
                if o.type==ARM_OP_IMM: imm=o.imm
            if imm is None: continue
            pc=(ins.address+4)&~3 if mode==CS_MODE_THUMB else (ins.address+8)&~3
            for sign,mn in [(1,ins.mnemonic)]:
                if ins.mnemonic in ("sub",): 
                    t=pc-imm
                else:
                    t=pc+imm
                if t in tv:
                    out.append((ins.address, ins.mnemonic, imm, t, tv[t]))
    return out
for mode,name in [(CS_MODE_ARM,"ARM"),(CS_MODE_THUMB,"THUMB")]:
    r=scan(mode)
    print(f"=== {name}: {len(r)} refs to target strings ===")
    for a,mn,imm,t,s in r[:40]:
        print(f"  {a:#010x}: {mn} #{imm:#x} -> {t:#010x}  {s!r}")
