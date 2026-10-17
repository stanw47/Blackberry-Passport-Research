import re
p = r"C:\Users\Stanley Williamson\Desktop\Blackberry-Passport-Research\recon\passport-android-dump\partition-dumps\aboot.img"
d = open(p, "rb").read()
kw = [b"unlock",b"Unlock",b"UNLOCK",b"locked",b"Locked",b"is_unlocked",b"flashing",
      b"fastboot",b"oem ",b"OEM",b"factory",b"Factory",b"verified",b"Verified",b"verity",
      b"dm-verity",b"secure boot",b"SecureBoot",b"TARGET_BUILD_VARIANT",b"production key",
      b"debug",b"Debug",b"token",b"Token",b"devinfo",b"getvar",b"flash:",b"switchboot",
      b"littlekernel",b"LittleKernel",b"lk_",b"aboot",b"bootloader",b"Bootloader",
      b"orange",b"orange_state",b"green",b"FACTORY",b"UNLOCKED"]
low = d.lower()
for k in kw:
    idxs = [m.start() for m in re.finditer(re.escape(k.lower()), low)][:8]
    if idxs:
        print(f"{k.decode():<22} {len(idxs)} hit(s): " + ", ".join(hex(i) for i in idxs))
