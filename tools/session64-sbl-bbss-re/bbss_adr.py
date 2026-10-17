import re, struct
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_ARM
from capstone.arm import ARM_OP_REG, ARM_OP_MEM, ARM_OP_IMM, ARM_REG_PC
p = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\vendor\backup_bootchain\parts_secure\bbss.img"
d = open(p,"rb").read(); base=0xf8050000
def str_at(t):
    if base<=t<base+len(d):
        m=re.match(rb"[\x20-\x7e]{4,}", d[t-base:t-base+64])
        if m: return m.group().decode()
    return None
want = ["Boot partition: %d","default BOOT0","Loading %s SBL","Failed to find the SBL partition",
        "PRIMARY","BACKUP","Authenticating SBL","SBL authentication failed","EFI PART",
        "read GPT header failed","No matching guid","No matching partition","Find %s part failed",
        "bootcontrolblock","update-bootchain","BCB indicates update","Backup boot key combo"]
def lit(t):
    if base<=t<base+len(d)-4:
        return struct.unpack_from('<I',d,t-base)[0]
    return None
def scan(mode):
    md=Cs(CS_ARCH_ARM, mode); md.detail=True
    step=2 if mode==CS_MODE_THUMB else 4
    out=[]
    for off in range(0x50, len(d)-4, step):
        for ins in md.disasm(d[off:off+4], base+off):
            pc=(ins.address+4)&~3 if mode==CS_MODE_THUMB else (ins.address+8)&~3
            if ins.mnemonic=="adr":
                imm=ins.operands[1].imm if len(ins.operands)>1 else 0
                t=pc+imm; s=str_at(t)
                if s and any(w in s for w in want): out.append((ins.address,"adr",imm,t,s))
                continue
            # ldr rX,[pc,#imm]  -> literal pool
            if ins.mnemonic in ("ldr","ldr.w") and len(ins.operands)==2 and ins.operands[1].type==ARM_OP_MEM and ins.operands[1].mem.base==ARM_REG_PC:
                t=pc+ins.operands[1].mem.disp
                v=lit(t)
                s=str_at(t) or ""
                sv=str_at(v) if v else None
                if sv and any(w in sv for w in want): out.append((ins.address,"ldr-literal",t,v,sv))
                elif s and any(w in s for w in want): out.append((ins.address,"ldr-addr",t,t,s))
                continue
            # add rX, pc, #imm
            if ins.mnemonic.startswith("add") and any(o.type==ARM_OP_REG and o.reg==ARM_REG_PC for o in ins.operands):
                imms=[o.imm for o in ins.operands if o.type==ARM_OP_IMM]
                if imms:
                    t=pc+imms[-1]; s=str_at(t)
                    if s and any(w in s for w in want): out.append((ins.address,"add-pc",imms[-1],t,s))
    return out
for mode,nm in [(CS_MODE_THUMB,'THUMB'),(CS_MODE_ARM,'ARM')]:
    r=scan(mode)
    print(f"===== {nm}: {len(r)} refs =====")
    for a,kind,imm,t,s in sorted(set(r)):
        print(f"  {a:#010x}: {kind:<12} {t:#010x}  {s[:55]!r}")
