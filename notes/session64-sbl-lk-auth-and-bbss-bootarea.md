# Session 64 — Does the SBL authenticate the LK? (BBSS→SBL→LK trusted boot)

Date 2026-10-18. Follows session63 (flash-after-repair + HLOS-verify). Device:
oslo Android prototype (`oslorow`, AAA787). Question posed at the end of session63:
**does the SBL authenticate the APPSBL/LK, and is a user-area `aboot` write alone
enough to unlock?** Answer below is **no** on both counts.

Artifacts (committed this session): `recon/passport-android-dump/vendor/backup_bootchain/`
(`parts_secure/`, `parts_insecure/`, both `backup_bootchain_*.pkg`), plus the
session62 pull dumps and session26 `ptracecow_big` (see the session63 dump commit).

--------------------------------------------------------------------------------
## 0. TL;DR

- **The SBL authenticates the images it loads (incl. the LK).** `parts_secure/sbl1r.img`
  carries `boot_authenticator.c`, `Image Load, Start` and `[31mSHA256 auth failure!`,
  and its load/region table (`file 0x3154c`) stages **`APPSBL`** (the LK) alongside
  RPM/TZ/WDT/DATARAM/MSGRAM.
- **BBSS loads the SBL from the eMMC boot partition (boot area), not from the user
  area.** BBSS is a Thumb loader (base `0xf8050000`); it detects the active boot
  partition, reads **that partition's GPT**, finds the SBL partition **by type GUID**,
  and authenticates it before jumping.
- **The "insecure" SBL does NOT skip authentication.** `parts_secure/sbl1r.img` and
  `parts_insecure/sbl1r.img` differ **only in data / relocations** (trust anchors:
  CA sets, image UUIDs, `AAA625`→`AAA624`), never in code — so the insecure chain
  still verifies, just against the insecure anchor set.
- **Consequence:** the boot chain the device runs is the **boot-area** one
  (`boot_gpt_*.bin`: `hwi, bbss, sbl1r, tzr, rpmr, sdir, abootr`). The user-area
  `sbl1`/`aboot` (the 40-partition `user_gpt`) are a *separate copy*. A user-area
  `aboot` write alone therefore cannot change the authenticated chain → the unlock
  needs a boot-area write (BootROM/ramloader lane), not an OS-only write.

--------------------------------------------------------------------------------
## 1. BBSS reverse-engineering (parts_secure/bbss.img, 262 144 B)

Header: `d1 dc 4b 84 | 34 10 d7 73 | 15 00 00 00 | ff ff ff ff | ff ff ff ff |
50 00 00 00 | 00 00 05 f8 …` → magic `0xd1dc4b84` (same as SBL1), code size
`0x1154c`, **load base `0xf8050000`**, header size `0x50`. Code is **ARM/Thumb-2
(Thumb)**; string addresses are built with Thumb `adr` (not literal pools).

Strings of interest: `bbss_log`, `BBSS started`, `Version: %s`, `pmicInit failed!`,
`mmcInit failed!`, `EFI PART`, `MMC: read GPT header failed`, `Invalid GPT header`,
`Failed to read GPT block %d`, `boot partition detection failure; default BOOT0`,
`Boot partition: %d`, `nvuser`, `BCB indicates update is available`,
`Loading %s SBL` (`PRIMARY`/`BACKUP`), `Failed to find the SBL partition`,
`Failed to read the SBL header`, `Invalid SBL header`, `Authenticating SBL`,
`SBL authentication failed!`, `Primary SBL version of %u is too old, expecting %u`,
`sbl_anti_rollback_ver`, `Failed to load primary SBL, will load backup SBL instead`,
`Failed to load backup SBL`, `Jump to SBL`, `SBL returned!`, `Backup boot key combo
active`, `Debug board switch forcing backup boot chain`, plus a full **vtnvfs**
implementation, a **SHA512** transform, and the certs `BB Root CA (secure)` /
`BB Attestation CA (secure)`.

### 1.1 Main flow (`bbss_main`, file ~0x190)

```
log "Version: %s"
pmicInit (0xf8051332)            -> "pmicInit failed!"
mmcInit  (0xf8050da0)            -> "mmcInit failed!"
boot_part = detect_boot_partition(0xf8050f1c)(&[sp])   ; default [sp]=1, log "Boot partition: %d"
bsi_create (0xf8051b6e)          ; build-info / HWI
bsi_update_with_build_config (0xf80502a6)
vtnvfs init (0xf805463a); mount "nvuser"; read version
check_BCB (0xf805475c)           -> "BCB indicates update is available"  (sets r4=1)
check_backup_key_combo (0xf80513de) -> "Backup boot key combo active"    (sets r4=1)
if (!r4) { bbss_load_sbl(PRIMARY=0); if fail log "Failed to load primary SBL, will
           load backup SBL instead"; }
bbss_load_sbl(BACKUP=1)          ; fallback on the inactive boot partition
"Jump to SBL" / "SBL returned!"
```

### 1.2 Boot-partition detect — `0xf8050f1c(&idx)`

Iterates **3 MMC handles** (`handles[i]`, `i=0..2`), calls `mmcGetDeviceInfo`
(`0xf805d454`), and tests a byte at **device-info +0x24**. The first handle whose
flag is set is written to `*idx` and the function returns 1; otherwise it logs
`boot partition detection failure; default BOOT0` and the caller defaults `[sp]=1`.
So BBSS selects **which eMMC boot partition (boot area) it is running from / will
boot from** — i.e. the *hardware* boot partition, not a user-area GPT.

### 1.3 GPT read + partition search — `0xf8050f60(part_idx, guid, out_num_sec)`

- `mmcRead(0xf8050e74)` reads the GPT header from partition **`part_idx`** (the value
  from §1.2 — the boot partition), then memcmp's the first 8 bytes against
  `EFI PART` (adr `0xf80517ec-adjacent`, `0xf8050f88`). Mismatch → `Invalid GPT
  header` / `MMC: read GPT header failed`.
- It then walks the partition-entry array (`num_entries` from the header at
  `[sp+0x50]`), and for each entry does a **16-byte `memcmp` of the entry's type GUID
  against the caller-supplied GUID** (`0xf80575a8`). On a match it returns
  `first_lba = [entry+0x20]` and, if `out` is non-null, `num_sectors =
  last_lba-first_lba+1`.
- `0xf8050e74(handle_idx, 1, block, dest)` is the generic `mmcRead`: `cmp r0,#3`
  bounds the partition index, looks up `handles[idx]`, calls the raw read
  (`0xf805d734`); errors are `mmcRead: failed(%d)` / `Invalid args` / `Not initialized`.

### 1.4 SBL load + authenticate — `bbss_load_sbl(arg)` (`0xf8051680`)

`arg==0` → name `"PRIMARY"`, `arg==1` → `"BACKUP"` (adr at `0xf805169e`/`0xf80516ae`).

```
detect_boot_partition -> idx (default 1, "default BOOT0")
find SBL partition   : 0xf8050f60(idx, sbl_guid, 0)   ; GPT of the BOOT PARTITION
  fail -> "Failed to find the SBL partition"
read SBL header (0x50 B); validate magic/len/fields -> "Failed to read/Invalid SBL header"
read SBL image       -> "Failed to read SBL image"
log "Authenticating SBL"; bl 0xf8051930 (authenticator)
  fail -> "SBL authentication failed!"   (secure BBSS: returns 0 = hard fail)
anti-rollback: actual ver vs expected (0xf8051664); if actual < expected, for the
  PRIMARY only -> "Primary SBL version of %u is too old, expecting %u" (0xf80517ba)
persist sbl_anti_rollback_ver (0xf8051b84)
```

`0xf8051930` is the signature verifier; BBSS embeds the **secure** CA chain
(`BB Root CA (secure)`, `BB Attestation CA (secure)`, SHA-512). In the **insecure**
build the failure branch instead logs `insecure device; ignoring SBL auth failure!`
(string only present in `parts_insecure/bbss.img`).

### 1.5 The two GPTs (parsed from `F:\imggen`)

`boot_gpt_secure.bin` / `boot_gpt_insecure.bin` — a **4 MiB** GPT
(`backup_lba=8191`, `first_usable=34`), i.e. for an eMMC **boot partition**:

| # | insecure         | secure           | type GUID (low bytes)            | size |
|---|------------------|------------------|----------------------------------|------|
| 0 | `hwi`            | `hwi`            | 7a05b304-…-60685F9BE70D          | 4 K  |
| 1 | `bbss`           | `stage1`         | dea0ba2c-…-F428251C3E98/9        | 128 K|
| 2 | `sbl1r`          | `stage2`         | 65f98a0e-…-C244D1105898/7        | 320 K|
| 3 | `tzr`            | `stage3`         | a053aa7f-…-2F68AC71A4F4/5        | 8 K / 512 K |
| 4 | `rpmr`           | `bbss`           | 098df793-…-89D711772228          | 256 K|
| 5 | `sdir`           | `sbl1r`          | d4e0d938-…-BC5ED5C4B203          | 64 K |
| 6 | `abootr`         | `tzr`/`rpmr`/`sdir`/`abootr` | 400ffdcd-…-F16ED9382388 | 2 M  |

`user_gpt.bin` — the **user area** GPT (40 entries, `backup_lba=61071359` ≈ 32 GB):
`gsign, mct_b, pad, nvram, calwork_b, dmi_b, calback_b, **aboot**, **sbl1**, rpm,
tz, sdi, fsc, modemst1/2, blog, perm, nvuser, ddr, bkup_ddr, prdid, prdidsig,
boardid, boardidsig, fsg, ssd, metadata, frp, bcota, rcause, spare, persist, crypto,
ares, boot, recovery, modem, system, cache, userdata`.

Note **`sbl1`** (user) and **`sbl1r`** (boot) carry the **same partition type GUID**
(`65f98a0e-…5898`), and **`aboot`**/**`abootr`** share `400ffdcd-…2388` — so the
distinction between "primary" and "backup" chains is purely **which GPT/device** is
read, and BBSS reads the **boot-partition GPT**.

--------------------------------------------------------------------------------
## 2. SBL authentication of the LK (parts_secure/sbl1r.img, 1 MiB, base 0xf800c000)

- Magic `d1 dc 4b 84`; load base **`0xf800c000`**, size `0x412ec`
  (`base+size = 0xf804d2ec` matches the 0x1c/0x24 fields). 297 literal `u32`
  string pointers resolve cleanly at this base.
- Auth evidence: `boot_authenticator.c` (fo `0x115a4`), `boot_config.c`
  (fo `0x11c88`/`0x11d3c`), `Image Load, Start` (fo `0x11c98`),
  `[31mSHA256 auth failure!` (fo `0x11cc1`), `Signature not found or invalid`.
- **Load/region table** at fo `0x3154c` (strides of 16 B, fields
  `{load_addr, size, name_ptr, delta_log_ptr}`) names the staged images:
  `APPSBL Image Loaded, Delta`, `RPM Image Loaded, Delta`, `TZ Image Loaded, Delta`,
  `WDT Image Loaded, Delta`, `DATARAM.BIN`, `MSGRAM.BIN`, `LPM.BIN`. **The APPSBL
  (LK) is in this table** → the SBL loads the LK, and the loader in `boot_config.c`
  is the one that emits `SHA256 auth failure!`.
- **secure vs insecure sbl1r diff = data only.** 498 differing bytes, all in
  literal pools / rodata: e.g. `0xf80308a4` `ldr r2,[pc,#0x50]`→`[pc,#0x00]`
  (pool `0xf8044a14`→`0xf8044a00`); anchor hashes @`0x38984`/`0x389e7`/`0x38a52`/
  `0x41364`; image UUID `8515E55B-9E4E-4321-A359-E77165B9F812` →
  `2E54B664-35BB-4D45-A95A-9FBD6D9A78C9` (@`0x3163c`); version `AAA625`→`AAA624`
  (@`0x31618`). **No control-flow change** → the insecure SBL still authenticates.

--------------------------------------------------------------------------------
## 3. What this means for the unlock

1. The running chain is **boot area**: `PBL → boot area (BBSS) → BBSS reads the
   boot-partition GPT → sbl1r → tzr/rpmr → abootr(LK)`.
2. The SBL **authenticates the LK** against an anchor embedded in the SBL; the
   gate-free engineering LK (`F:\imggen\aboot.mbn`) is a bare/unsigned MBN
   (no `BB Root CA`/`BB Attestation CA`/`AUTHBOOT`), so it cannot satisfy the
   secure anchor.
3. Therefore a **user-area `aboot` write alone does NOT unlock**: (a) the chain is
   read from the boot area, and (b) even if reached, an unsigned LK fails SBL auth.
4. To boot a custom LK you must land the **insecure anchor set** — the insecure
   BBSS/SBL — which live in the **boot area (boot0/boot1)**, matching the
   backup-bootchain package (`boot_gpt_*.bin` + `bbss/sbl1r/tzr/rpmr/sdir/abootr`).
   That is a boot-area write = the **BootROM/ramloader lane** (`boot0_flash.py`),
   not an OS-only primitive.

--------------------------------------------------------------------------------
## 4. Open items / next

- **Device LK auth path** (analysis 3): read `recon/.../partition-dumps/aboot.img`
  to confirm the LK's own HLOS-verification trust store and whether the boot-area
  `abootr` differs from the user-area `aboot` (they share the type GUID).
- **BootROM/ramloader lane** (analysis 4): `F:\Passport-Android\boot0-step\`,
  `boot0_flash.py`, `loader_8D002C0A-00.bin`, and the "0x2019 NVRAM unlock" gate —
  the only path that reaches the boot area without desoldering.
- Confirm the **user-area `sbl1`/`aboot`** purpose (separate copy written by the
  backup-bootchain/OTA mechanism, vs. a real boot source under a different BPE).

--------------------------------------------------------------------------------
## 5. Device LK (`partition-dumps/aboot.img`, 4 MiB) — HLOS trust store / auth

The production LK **does** verify the HLOS; the gate-free engineering LK does not.
Evidence (strings clustered at file `0xb0000-0xb9000`):

- **Named per-product key ids:**
  `AUTHBOOT-{keianrow,oslorow,wolverineemea,wolverinena,wolverinesprint,wolverinevzw}`,
  `DBGSIG-*`, `DDT-*`, `NVSIG-*`, `STP-*`. `AUTHBOOT-oslorow` is the production
  HLOS-verification key for *this* device.
- **Cert chain:** `BB Root CA (secure)` / `BB Attestation CA (secure)` (several
  copies), X.509 DER + `SHA256`/`SHA1`/`RSA` (Security Builder), consistent with a
  UEFI-style signed image / hash-table verification.
- **HLOS verify path:** `/perm/hlos_signature.tkn`, `/nvuser/hlos_unsigned.tkn`,
  `/nvuser/dbg_console.tkn`, `/nvuser/pathtrust_mode.tkn`, `/nvuser/selinux_mode.tkn`,
  `/nvuser/perf_config.tkn`; messages `Signature not found!`,
  `Boot image verification failed (production key)`,
  `Attempting HLOS image verification with \"%s\" key`,
  `Key name in debug token does not match with boot signature`,
  `Debug token hlos_unsigned is active. Skipping image verification.` and crucially
  **`HLOS Image verification failed on insecure build`** → on an **insecure build the
  LK tolerates HLOS-verify failure** (does not hard-reboot).
- **Debug tokens:** `/perm`, `nvuser` token parser, `Token PIN %08X does not match
  this device PIN:%08X`, `Token not for this device, bad procid` → tokens are
  device-bound; no servers, so not a route.
- **Blacklist / anti-rollback:** `blacklist`, `blacklist_certificate`,
  `Bootchain fuse validation disabled in blocklist`, `Validating SBL against blacklist`,
  `OS does not pass blacklist check`, `sbl_anti_rollback_ver`, RPMB + NV
  (`Failed to write blacklist meta to RPMB`).
- **Boot-area partitions named in the LK:** `phyboot0`, `phyboot0hwi`, `phyboot1hwi`
  alongside `oemspare, modemst1/2, gsign, mct_b, nvram, calwork_b, dmi_b,
  calback_b, bkup_ddr, prdidsig, boardidsig, rcause, ares, crypto, metadata, bcota`
  (matches the user-area GPT; confirms the `oem switchboot` / backup-bootchain targets).
- The **engineering LK** (`F:\imggen\aboot.mbn`) contains **none** of
  `AUTHBOOT`/`BB * CA`/`HLOS`/`token`/`blacklist` → it boots *any* HLOS. This is
  precisely why it is the unlock component — but it must first be *loaded by the SBL*,
  which requires the insecure SBL/anchor (§2/§3). **Caveat:** the device's own
  `backup_bootchain_insecure.pkg` `abootr` (`b5039e53…`) is *not* this LK — it still
  carries `AUTHBOOT`×12 + secure CAs (BlackBerry's "insecure" backup chain ≠ the
  engineering chain).
