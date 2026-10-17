import os, re
from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM
from capstone.arm import ARM_OP_REG, ARM_OP_IMM, ARM_REG_PC
bb  = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\vendor\backup_bootchain"
p = os.path.join(bb,"parts_secure","sbl1r.img")
d = open(p,"rb").read()
base = 0xf800c000
period = len(d)
keyword = re.compile(rb"(auth|Auth|insecure|Insecure|fusemap|APPSBL|bind|bind)", re.I)

def str_at(fo):
    if 0 <= fo < len(d):
        m = re.match(rb"[\x20-\x7e]{4,}", d[fo:fo+64])
        if m: return m.group().decode()
    return None

md = Cs(CS_ARCH_ARM, CS_MODE_ARM)
md.detail = True
found = []
# disassemble 4-byte aligned
for off in range(0x50, len(d)-4, 4):
    for ins in md.disasm(d[off:off+4], base+off):
        ops = ins.operands
        if not ops: continue
        mn = ins.mnemonic
        if mn not in ("add","sub","adr","mov","ldr"): continue
        has_pc = any(o.type==ARM_OP_REG and o.reg==ARM_REG_PC for o in ops)
        if not has_pc: continue
        imm=None
        for o in ops:
            if o.type==ARM_OP_IMM: imm=o.imm
        if imm is None: continue
        pc = (ins.address + 8) & ~3
        t = pc + imm if mn in ("add","adr","mov") else pc - imm
        if base <= t < base+period:
            s = str_at(t-base)
            if s:
                found.append((ins.address, mn, imm, t, s))

seen=set()
for a,mn,imm,t,s in found:
    if (a) in seen: continue
    seen.add(a)
    if keyword.search(s.encode()) or "Image" in s or "SBL" in s:
        print(f"{a:#010x}: {mn} {imm:#x} -> {t:#010x}  {s[:70]!r}")
print(f"\ntotal computed-string refs: {len(found)}; keyword-refs printed above")
