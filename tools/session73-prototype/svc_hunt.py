import re, os, glob

RAM = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-boot-dumps\ramdisk-text"
ROOT = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\root"

# 1) parse file_contexts -> map path->label (exact + regex where simple)
fc = {}
defaults = []
for line in open(os.path.join(ROOT, "file_contexts"), encoding="utf-8", errors="replace"):
    line = line.strip()
    if not line or line.startswith("#"): continue
    parts = line.split()
    if len(parts) < 2: continue
    path, lab = parts[0], parts[1]
    fc[path] = lab

def label_of(binpath):
    if binpath in fc: return fc[binpath]
    # try prefix/wildcard defaults
    for p, lab in fc.items():
        if p.endswith("(/.*)?") and binpath.startswith(p[:-6]): return lab
        if p.endswith("(/.*)") and binpath.startswith(p[:-5]): return lab
    return "(unlabeled -> system_file default?)"

# 2) parse init*.rc -> services (name, bin, seclabel)
svc = []
for f in glob.glob(os.path.join(RAM, "init*.rc")):
    cur = None
    for line in open(f, encoding="utf-8", errors="replace"):
        s = line.strip()
        m = re.match(r"service\s+(\S+)\s+(\S+)", s)
        if m:
            cur = {"name": m.group(1), "bin": m.group(2), "seclabel": None, "file": os.path.basename(f)}
            svc.append(cur); continue
        if cur is not None:
            ms = re.match(r"seclabel\s+(\S+)", s)
            if ms: cur["seclabel"] = ms.group(1)
            if s.startswith("service ") or s.startswith("on "):
                if not s.startswith("service "): cur = None

print("services parsed:", len(svc))
print("\n== services whose binary is NOT *-exec labelled (-> system_file, shell-Dirty-COWable) ==")
for c in svc:
    b = c["bin"]
    if not b.startswith("/system/bin/"): continue
    lab = label_of(b)
    if "_exec:" in lab or lab.startswith("u:object_r:") and lab not in ("u:object_r:system_file:s0",):
        # labelled specially -> skip
        continue
    print("  %-22s %-34s seclabel=%s  [%s]" % (c["name"], b, c["seclabel"], c["file"]))
