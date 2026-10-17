import os
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
bb  = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\vendor\backup_bootchain"
sec = open(os.path.join(bb,"parts_secure","sbl1r.img"),"rb").read()
ins = open(os.path.join(bb,"parts_insecure","sbl1r.img"),"rb").read()
base = 0xf800c000
print(f"sizes {len(sec)} vs {len(ins)}")
diffs = [i for i in range(min(len(sec),len(ins))) if sec[i]!=ins[i]]
print(f"{len(diffs)} differing bytes")
# group into ranges
ranges=[]
for i in diffs:
    if ranges and i-ranges[-1][1]<=4: ranges[-1][1]=i
    else: ranges.append([i,i])
for a,b in ranges:
    print(f"  range {a:#07x}-{b:#07x} ({b-a+1} bytes): sec={sec[a:b+1].hex()} ins={ins[a:b+1].hex()}")

md=Cs(CS_ARCH_ARM, CS_MODE_THUMB); md.detail=False
r0,r1 = ranges[0]
lo=(r0-0x40)&~1
print(f"\n=== THUMB disasm around first diff ({lo:#x}-{r1+0x40:#x}) ===")
print("  -- SECURE --")
for x in md.disasm(sec[lo:r1+0x40], base+lo): print(f"   {x.address:#010x}: {x.mnemonic:<8} {x.op_str}")
print("  -- INSECURE --")
for x in md.disasm(ins[lo:r1+0x40], base+lo): print(f"   {x.address:#010x}: {x.mnemonic:<8} {x.op_str}")
