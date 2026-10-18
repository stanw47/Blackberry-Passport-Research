from capstone import *
p = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\emmc-full-backup\dumps\aboot.img"
d = open(p, "rb").read()
BASE = 0x100c4000
md = Cs(CS_ARCH_ARM, CS_MODE_ARM)
lo, hi = 0x17400, 0x17b40
print(f"=== disasm {lo:#x}-{hi:#x} (VA {BASE+lo:#x}) ===")
for ins in md.disasm(d[lo:hi], BASE+lo):
    mark = ""
    if ins.mnemonic.startswith("bl") or ins.mnemonic in ("cbz","cbnz"):
        mark = "   <-- CALL/BRANCH"
    print(f"{ins.address:#010x}: {ins.bytes.hex():<10} {ins.mnemonic:<7} {ins.op_str}{mark}")
