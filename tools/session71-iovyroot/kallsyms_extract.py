import re, struct
vm = open(r"C:\Users\STANLE~1\AppData\Local\Temp\opencode\vm_4698.bin","rb").read()
base = 0xc0008000
# names in order (from the device /proc/kallsyms capture)
names = []
for line in open(r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\proc\kallsyms.txt", encoding="utf-8", errors="replace"):
    m = re.match(r"^[0-9a-fA-F]{8} (\S) (.*)$", line.rstrip("\n"))
    if m:
        names.append((m.group(1), m.group(2)))
print("names:", len(names))
N = len(names)

# find kallsyms_addresses: a u32 array (absolute addrs) of length ~N, increasing, in kernel range
def find_addr_array():
    best = None
    # scan for a u32 equal to a plausible .text start then check monotonic run
    step = 4
    for off in range(0, len(vm)-4, step):
        v = struct.unpack_from("<I", vm, off)[0]
        if not (base <= v < 0xc1200000):
            continue
        # try to walk a monotonic run
        run = 1; prev = v
        o = off + 4
        while o < len(vm)-4 and run < 40:
            x = struct.unpack_from("<I", vm, o)[0]
            if not (prev <= x < 0xc1200000):
                break
            prev = x; run += 1; o += 4
        if run >= 30:
            # candidate start; estimate total run
            o2 = off; prev = -1; cnt = 0
            while o2 < len(vm)-4:
                x = struct.unpack_from("<I", vm, o2)[0]
                if not (base <= x < 0xc1200000) or x < prev:
                    break
                prev = x; cnt += 1; o2 += 4
            if best is None or cnt > best[1]:
                best = (off, cnt)
    return best

# full monotonic run search (robust): find longest increasing u32 run in range
def longest_run():
    best = (0,0,0)
    i = 0; n = len(vm)//4
    while i < n:
        v = struct.unpack_from("<I", vm, i*4)[0]
        if base <= v < 0xc1200000:
            j = i+1; prev = v
            while j < n:
                x = struct.unpack_from("<I", vm, j*4)[0]
                if not (base <= x < 0xc1200000) or x < prev:
                    break
                prev = x; j += 1
            if j-i > best[1]-best[0]:
                best = (i*4, j*4, j-i)
            i = j
        else:
            i += 1
    return best

st, en, cnt = None, None, 0
b = longest_run()
st, en, cnt = b
print("longest increasing u32 addr run: off %#x..%#x  count=%d (names=%d)" % (st, en, cnt, N))

# alignment check: kallsyms_addresses has N entries; the run we found should start at the array.
if cnt >= N:
    addrs = [struct.unpack_from("<I", vm, st + i*4)[0] for i in range(N)]
    target = {}
    for i,(t,nm) in enumerate(names):
        if nm in ("uevent_helper","modprobe_path","selinux_enabled","selinux_enforcing",
                  "sys_call_table","commit_creds","prepare_kernel_cred","pathtrust_enforce",
                  "init_cred","init_task","kptr_restrict","selinux_ops"):
            target[nm] = addrs[i]
    for k,v in target.items():
        print("  %-22s %#x" % (k, v))
    print("  first sym:", names[0], hex(addrs[0]))
    print("  last  sym:", names[-1], hex(addrs[-1]))
else:
    print("run shorter than names; need different approach (off=%#x cnt=%d)" % (st, cnt))
