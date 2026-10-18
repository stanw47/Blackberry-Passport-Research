from capstone import *
p = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\emmc-full-backup\dumps\aboot.img"
d = open(p, "rb").read()
BASE = 0x100c4000
md = Cs(CS_ARCH_ARM, CS_MODE_ARM)
def dis(lo, hi, title):
    print(f"===== {title}  file {lo:#x}..{hi:#x}  VA {BASE+lo:#x} =====")
    for ins in md.disasm(d[lo:hi], BASE+lo):
        c=""
        if ins.mnemonic in ("bl","blx") or ins.mnemonic.startswith("b") or ins.mnemonic.startswith("cb"):
            c="  <<"
        print(f"{ins.address:#010x}: {ins.bytes.hex():<10} {ins.mnemonic:<8} {ins.op_str}{c}")
dis(0x17740, 0x178c0, "around verifier call")
dis(0x178c0, 0x179a0, "after")
