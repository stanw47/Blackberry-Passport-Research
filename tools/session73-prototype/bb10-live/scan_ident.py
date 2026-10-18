import os, re, hashlib, json
D = r"F:\Passport-Originals"
files = ["orig_boot0.bin","orig_boot1.bin","orig_user0_128M.bin",
         "part_nvram0.bin","part_cal_work0.bin","part_dmi0.bin","part_uda0.bin","part_radio0.bin"]

def luhn(s):
    t=0
    for i,ch in enumerate(reversed(s)):
        d=int(ch)
        if i%2==1:
            d*=2
            if d>9: d-=9
        t+=d
    return t%10==0

print("=== SHA256 + markers ===")
rows=[]
for f in files:
    p=os.path.join(D,f)
    if not os.path.exists(p): print(f,"MISSING"); continue
    d=open(p,"rb").read()
    h=hashlib.sha256(d).hexdigest()
    imei_marker = (b"IMEI" in d) or (b"imei" in d)
    bsn_marker = (b"BSN" in d) or (b"bsn" in d)
    # ASCII 15-digit Luhn IMEI candidates
    cand=set()
    for m in re.finditer(rb"(?<![0-9])(\d{15})(?![0-9])", d):
        s=m.group().decode()
        if s[0] in "358" and luhn(s): cand.add(s)
    bb = (b"BlackBerry" in d) or (b"WINDERMERE" in d) or (b"SQW100" in d) or (b"windermere" in d)
    rows.append((f,len(d),h,imei_marker,bsn_marker,sorted(cand)[:6],bb))
    print(f"{f:24} {len(d):>10}  IMEI={int(imei_marker)} BSN={int(bsn_marker)} BB={int(bb)}  ascii_imei={sorted(cand)[:6]}")

print("\n=== strings in nvram0 / dmi0 / uda0 (identity-ish) ===")
for f in ("part_nvram0.bin","part_dmi0.bin","part_uda0.bin"):
    d=open(os.path.join(D,f),"rb").read()
    print("##",f)
    for m in list(re.finditer(rb"[\x20-\x7e]{6,}", d))[:25]:
        print("   %#08x %s" % (m.start(), m.group().decode("latin1")[:100]))
