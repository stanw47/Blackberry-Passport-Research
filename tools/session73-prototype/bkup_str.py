from capstone import *
p = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\tools\session62-bcota-android\pulled\bkup_bc_update"
d = open(p,"rb").read()
TEXT_OFF=0x784; TEXT_VA=0x784; TEXT_LEN=0x73c
RO_OFF=0xee8
def ro(va):
    if RO_OFF <= va < RO_OFF+0x6d7:
        s=d[va:va+80].split(b"\x00")[0]
        return s.decode("latin1","replace")
    return None
# find all "ldr.w rX,[pc,#imm]" then the next "add rX,pc"
import re
md=Cs(CS_ARCH_ARM, CS_MODE_THUMB); md.detail=True
text=d[TEXT_OFF:TEXT_OFF+TEXT_LEN]
ins=list(md.disasm(text, TEXT_VA))
pool_use={}   # addr -> (reg, pooladdr)
for i,insn in enumerate(ins):
    if insn.mnemonic=="ldr.w" and "[pc" in insn.op_str:
        m=re.search(r"r(\d+), \[pc, #(\d+)\]", insn.op_str)
        if m:
            reg=int(m.group(1)); imm=int(m.group(2))
            # target pool address (thumb: align(pc,4)+imm), pc=insn.address+4
            pc=(insn.address+4)&~3
            pool=pc+imm
            pool_use[insn.address]=(reg,pool)
# for each pool load, find a nearby 'add rX, pc'
strs={}
for addr,(reg,pool) in pool_use.items():
    if not (TEXT_VA <= pool < TEXT_VA+TEXT_LEN): continue
    val=int.from_bytes(d[pool:pool+4],"little")
    for insn2 in ins:
        if insn2.address>addr and insn2.address<addr+16 and insn2.mnemonic=="add" and insn2.op_str==f"r{reg}, pc":
            tgt=(insn2.address+4)+val
            s=ro(tgt)
            if s is not None:
                strs[hex(addr)]=(hex(tgt),s)
for k,v in sorted(strs.items()):
    print(k,"->",v)
