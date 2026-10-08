# Session 32 — Autoloader compiler: `.exe` format decoded, packer proven byte-identical

Date: 2026-10-08. Follows session30/31. Goal: the **autoloader compilation
step** — be able to build/modify BB10 autoloaders ourselves.

## 1. The autoloader `.exe` format (verified byte-level)

```
[cap.exe bytes up to PE_end]            P = end of last PE section
[u32 0x97C5D59C] x3                     (12 B)
[80 x 0x00]
[u32 count]                             offset 0x5C from P
[count x { u32 0, u32 offset }]         offset = P + 0x84 + cumulative size (LE u32)
[zero padding to P + 0x84]
[file data, sequential]
```
- `cap.exe` = PE stub whose `PE_end` for our build is `0x8D2E00`.
- Header is a fixed `0x84` bytes from `P` to the first file; supports ≤4 files
  (32-bit offsets).
- Note: bb10mt's Pascal `MakeAutoloader` writes *16-byte* table entries
  (`[u64 0][u64 off]`) — that does **not** match real autoloaders; this port
  reproduces the real 8-byte entry form.

## 2. Proof — byte-identical repacks

| package | size | sha256 |
|---|---|---|
| stock rooted autoloader (`…root_v2.exe`) | 3,059,757,908 | `19116fb7…` |
| **repacked** (cap + v2.0.signed + v2.1.signed) | 3,059,757,908 | `19116fb7…` ✅ |
| balika Android (`Passport_Android_balika_v2.exe`) | 872,231,756 | `66f4e587…` |
| **repacked** (cap + v2.0-android.signed + radio) | 872,231,756 | `66f4e587…` ✅ |

Contents: `root_v2.exe` = cap + `root_v2.0.signed` + `root_v2.1.signed` (radio);
Android exe = cap + `v2.0-android.signed` + the **same radio record**
(sha256 of the Android exe's second file == `root_v2.1.signed`).

## 3. The cap.exe stub is the *patched* one everywhere

`working/shroot/cap.exe`, the stub inside `root_v2.exe`, and the stub inside the
Balika Android exe are **all identical** (sha256 `944b6816…`) and carry the
3-byte root patch (session9b):
- `0xC913`: `84 C0` → `38 C0` (signature dispatch always taken — unsigned QCFM accepted)
- `0x044959`: `mov eax,0x6C` → `mov eax,0`
- `0x044977`: `mov eax,0x49` → `mov eax,0`

⇒ the Balika Android autoloader is itself built on the rooted stub; that is why
it flashes on a rooted device.

## 4. Tool + next

- Tool: `tools/autoloader/autoloader_pack.py` (`pack <cap.exe> <out.exe> <in...>`).
- **Next**: QCFM/`.signed` packer (port of `qcfm.pas: packMFCQ`) so we can craft
  custom record sets (custom IFS/RFS/UFS/MBR), then build custom autoloaders.
- **Gate unchanged**: no QCFM record type targets `boot0`/`boot1` — the
  boot-region write still needs the loader path or hardware (session29/30/31).

## 5. Update — QCFM (`.signed`) packer also proven (same day)

Port of `qcfm.pas: _packMFCQ` (ver=2, fast mode) → `tools/autoloader/qcfm_pack.py`.
Layout confirmed against the balika container:

```
@0x00 mhf1 (32B): 'mfcq',0,ver=1,nheaders=0,headersz=data_off,0,flags=0x20,0
@0x20 mhf2 (28B): 'mfcq',0,ver=0x20000,length=28,nheaders=N,headersz,data_crc=0
@0x3C N x { cf2 (44B: 'pfcq',ver,length=60,_type,rrecOffset=44,nrec=1,
                 hwv=0,sig=0,blocksize=0x10000) + rr2 (16B: 'rrcq',len=16,
                 offset=0,count=ceil(size/0x10000)) }
[zeros to data_off = 0x5C + 60*N]
data blocks (files zero-padded to whole 64 KiB blocks)
```

**Proof**: extracted the balika `v2.0-android.signed` records
(IFS/RCFS/SIG2/MBR/UFS), repacked → **sha256 identical** (`55836c1b…`,
824,508,808 B).

⇒ We can now manufacture the full delivery chain ourselves:
records (`.signed`) → autoloader (`.exe`) → patched `cap.exe` → device updater.
This is the "adapt the OS/IFS/user image to the stock boot chain" toolset.
