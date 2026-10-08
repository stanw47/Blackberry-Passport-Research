# Session 30 — Three-way component diff: stock BB10 / Balika kit / prototype Android

Date: 2026-10-08. Goal: diff everything we hold from the three Android-relevant
sources to find the conversion's exact building blocks and any lead that avoids
writing the eMMC boot partitions.

Method: trimmed-byte equality (strip trailing `00`/`FF` padding) + full-file
md5, GPT tables, X.509/CA string fingerprints, build-info scan.

## Sources

- **Stock retail BB10**: `retail_boot0.img`, `retail_boot1.img` (blank),
  `retail_user_prefix.img`; stock autoloader records
  `passport-rooted/Passport_..._root_v2.0.{0.ifs,1.rcfs,3.mbr,4.ufs}`.
- **Balika**: `v2.0-android.signed` records; imggen **GitHub kit**
  (`priv-research/imggen/files/`); **prebuilt imggen** (`~/Downloads/imggen`,
  37 MB — embedded files; generated our `new_boot1.img`/`new_user_gpt.bin`);
  balika UFS payload (Android user area).
- **Prototype Android**: user-area dumps
  `recon/passport-boot-dumps/{aboot,sbl1,tz,rpm,sdi,pad}.img`;
  `backup_bootchain_{insecure,wp-power_on_secure}.pkg` + `parts_*`.

## A. Byte-identity map (trimmed)

| image | size (trim) | identical to | notes |
|---|---|---|---|
| kit `stage1.mbn` | 68,067 | **proto secure `bbss`** | secure CA set + `BBSS` |
| kit `stage2.mbn` | 267,242 | **proto secure `sbl1r`** | SBL1 build |
| kit `sbl1.mbn` | 271,385 | balika UFS `sbl1` | ≠ `sbl1r` by only **12 B @0x398cb** |
| kit `sbl1r.mbn` | 271,385 | (same as sbl1 minus 12 B) | |
| kit `tz.mbn` | 331,296 | balika UFS `tz` | proto `tz`/`tzr` same size, diff @0x84 |
| kit `rpm.mbn` | 191,311 | balika UFS `rpm` | proto `rpm`/`rpmr` same size, diff @0x1068 |
| kit `sdi.mbn` | 11,656 | **proto `sdi` == proto `sdir`** == balika UFS `sdi` | shared |
| kit `aboot.mbn` | 342,262 | balika UFS `aboot` | **no `AUTHBOOT`, no CA strings** |
| prebuilt-imggen `aboot` | 1,952,122 | — | also **no `AUTHBOOT`**; *different* build (common prefix `0xD`, kit aboot not a subset) |
| proto user-area `aboot` | 993,400 | — | `AUTHBOOT-*` + secure CAs |
| proto backup `abootr` (secure) | 972,044 | — | `AUTHBOOT-*` + secure CAs |
| proto backup `abootr` (insecure) | 972,044 | sec differs @0xae8bc | diff is code, **certs still secure CA set** |
| proto backup `sbl1r` (insecure) | 267,242 | sec differs @0x12034 | |
| proto backup `bbss` (insecure) | 69,120 | kit `bbss` = 69,096 (differs) ; sec = kit stage1 | third BBSS build |
| kit `bbss.mbn` | 69,096 | — | **insecure** CA set + `bbss_insecure` |
| kit `stage3.mbn` | 2,432 | ELF | built from `apps/pbl_flash.c` + `apps/pbl_mc.c` (= BBAndroids `passport_stage3`) |

Stock autoloader IFS/RCFS/MBR remain byte-identical to the balika package
(verified previously); stock UFS is the BB10 layout, balika UFS the Android one.

## B. Boot-partition GPT layouts

| source | partitions (in LBA order) |
|---|---|
| stock retail `boot0` | `BootROM` only (+ SBL1 image, MCT @0xffc00) |
| kit `boot_gpt_secure.bin` | hwi, stage1(128 K), stage2(320 K), stage3(8 K), bbss(128 K), sbl1r(320 K), tzr(512 K), rpmr(256 K), sdir(64 K), abootr(2 M) — **10 parts** |
| kit `boot_gpt_insecure.bin` | hwi, bbss(128 K), sbl1r(320 K), tzr(512 K), rpmr(256 K), sdir(64 K), abootr(2 M) — **7 parts** |
| prototype backup chain | hwi, padr1(7 K), bbss(256 K), sbl1r(1 M), tzr(512 K), rpmr(512 K), sdir(64 K), abootr(1 M), padr2 — **matches the insecure shape** |

Our generated `new_boot1.img` = **secure** variant (10 parts, from the prebuilt
imggen; all components verified == its embedded files, except the aboot — which
is the 1.95 MB build).

## C. Certificate / CA fingerprints

- **Secure CA set** (`BB Root CA (secure)` + `BB Attestation CA (secure)` +
  `Bryon Hummel` leaf): kit `stage1`; proto secure `bbss`/`abootr`; proto `aboot`
  (user area); proto insecure `abootr` (still secure CAs).
- **Insecure CA set**: kit `bbss`; proto insecure `bbss`.
- **No CA strings at all**: kit `aboot.mbn` (342 K), prebuilt-imggen aboot
  (1.95 M), balika UFS `aboot` → engineering/unsigned LK.
- `stage3.mbn` is the `passport_stage3` PBL-debug payload (not a security stage).

## D. Leads

1. **The kit is derived from the prototype's engineering build** — kit
   `stage1`/`stage2` are byte-identical to the prototype's *secure backup chain*
   `bbss`/`sbl1r`. The prototype's `backup_bootchain` pkgs are effectively the
   reference images the conversion kit was built from.
2. **The whole Android user-area chain is reproduced locally**: the balika UFS's
   `aboot`/`sbl1`/`rpm`/`tz`/`sdi` are byte-identical to the imggen kit files
   (and `aboot` is the AUTHBOOT-free engineering LK). Nothing is missing.
3. **The unlock component is the engineering LK**: both kit aboots have **no
   AUTHBOOT code** (vs the prototype's stock LK, which does). Installing the
   engineering `aboot` is what removes the fastboot authboot gate.
4. **The insecure boot GPT matches the prototype's backup-chain layout** (7
   parts, no stage1/2/3): the no-stages chain == the prototype's engineering
   chain shape. The secure GPT's stage1/2/3 are older/alternate components
   (stage1 = secure BBSS, stage2 = SBL1, stage3 = PBL-debug ELF).
5. **Two engineering LKs exist**: GitHub kit `aboot.mbn` (342 K) vs the prebuilt
   imggen's embedded aboot (1.95 M). We generated our boot image with the latter.
   Both lack AUTHBOOT; they are different builds (not prefix-related).
6. **Open**: does the chain verify the LK? The kit `aboot` carries no certs; the
   `bbss` strings show `insecure device; ignoring SBL auth failure!`. If the LK
   is verified, the engineering LK needs the **insecure** bbss path → assemble
   the insecure boot image (7-part GPT + kit `bbss`/`sbl1r`/`aboot`).

## E. Next diff steps

1. Extract the **prebuilt imggen**'s embedded `files/` (PyInstaller) to pin the
   exact 1.95 MB aboot and check whether other components differ from the GitHub
   kit.
2. Map the insecure-vs-secure code diffs (`bbss` @0x1c, `sbl1r` @0x12034,
   `abootr` @0xae8bc) to the "skip SBL auth" behaviour.
3. Diff the stock retail user-area MCT prefix against the imggen Android user GPT
   (the conversion's user prefix vs stock) — locates what the conversion must
   overwrite in the writable area.
4. Assemble the **insecure boot-partition image** from local components and
   compare it against the prototype's insecure backup payload (sanity check that
   we can rebuild BlackBerry's own engineering chain byte-for-byte, minus
   padding).

## F. Follow-ups (same day, 2026-10-08)

- **Secure-vs-insecure payload deltas** (prototype pkgs): `bbss` = 2–3 B
  @0x1c…0x2c + small patches; `sbl1r` = dozens of 1-B patches (@0x12034…);
  `abootr` = small patches + **32-B and 256-B regions** (@0xae8bc, 0x862884,
  0x869632 — signature/CA areas); `tzr` = 32 B @0x1128 + 1 B; `rpmr` = 64 B
  @0x1068 + 1-B patches; `sdir`/`hwi` **identical**. The "insecure" variant is
  the same chain with signature/flag-area patches.
- **GPT type-GUID scheme decoded** (low byte = role variant):
  `stage1 …251C3E98`, **`bbss` …251C3E99**, `stage2 …d110589**7**`,
  `sbl1r`/user `sbl1` `…d110589**8**`, `stage3`/user `tz` `…71a4**f4**`,
  `tzr …71a4f5`, `rpm(r) …177228`, `aboot(r) …382388`, `sdi(r) …c4b203`.
- **The `stage3` payload exploit is decoded and tracked** →
  `notes/session31-stage3-payload-exploit.md`. Short version: it re-enters the
  PBL in debug mode (`BOOT_PARTITION_SELECT=0x5D1`) with a **replacement PBL that
  skips SBL authentication** and loads the GPT partition with type GUID
  `…251C3E99` — the **secure** boot GPT's `bbss` partition (the insecure-CA BBSS).
- **Candidate insecure boot image assembled locally**: `boot_gpt_insecure.bin` +
  kit `hwi`(generated)/`bbss`/`sbl1r`/`tz`/`rpm`/`sdi`/`aboot` →
  `/tmp/opencode/new_boot0_insecure.img` (3,429,376 B, GPT CRCs recomputed).
  Shape matches the prototype's backup payload (same 7 roles; sizes/builds
  differ). Note: the imggen run for our retail produced the **secure** variant
  (10-part GPT incl. the exploit); this adds the 7-part insecure lane.
