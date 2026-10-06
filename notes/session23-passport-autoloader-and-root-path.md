# Session 23 — Passport autoloader boot-chain comparison + Android prototype root path

Date: 2026-10-06. Builds on `session22` (Passport Android prototype). Goal: use
the Passport autoloaders/dumps we already hold to (1) settle the signing
question, (2) map a flashable Android set, (3) push the prototype root hunt.

---

## 1. Signing — production PKI shared across every image we hold

Extracted the X.509 chain from each artifact and compared SHA-256:

| artifact | BB Root CA (secure) | BB Attestation CA (secure) | leaf |
|---|---|---|---|
| retail Passport BB10 `boot0` (`bb0_full.bin`) | `931828f4…` | `e55af2b1…` | `CN=Bryon Hummel` SW_ID `0x0` |
| Passport Android prototype (`backup_bootchain…/abootr.img`) | `931828f4…` | `e55af2b1…` | `CN=Bryon Hummel` SW_ID `0xC` |
| balika Android autoloader (`v2.0-android.signed`) | `931828f4…` | `e55af2b1…` | `495b0ed3…` (= prototype's aboot leaf) |

**All three use the SAME production "secure" PKI** (identical root + attestation
CA). There is **no separate engineering/test signing key** — the Android
prototype and the balika Android autoloader are signed with BlackBerry's
production image-signing key (leaf CN "Bryon Hummel", bhummel@blackberry.com).

Consequence: the prototype's Android images are **production-signed**, so a
retail Passport (fused with the production root key) should **accept** them.
The remaining blockers for a no-desolder conversion are writing the partitions
(EDL/ISP) and the eMMC boot-partition WP — **not** signature trust.

The balika Android autoloader carries **six** production leaves (all issued by
`BB Attestation CA (secure)`), keyed by Qualcomm SW_ID:
`495b0ed3` (Bryon Hummel, SW_ID 0xC — same as the prototype aboot),
`ddd17794` (Ian Melhuish, SW_ID 0x4), and `6946a98c` / `d0767d2e`
(`CN=BlackBerry`, `BBOS-Bootloader@blackberry.com`, SW_ID 0x1/0x2, DEBUG 0x2).
So the whole set is BlackBerry production-signed; the per-image SW_ID/DEBUG
fields differ but the root of trust is identical.

Extracted certs saved under `recon/passport-autoloader-certs/`.

## 2. Flashable Android set — balika autoloader vs prototype layout

The balika Android autoloader (`priv-research/autoloaders/passport-android/
v2.0-android.signed`, 824 MB) uses the **BlackBerry QCFM container** (`mfcq`
`/pfcq`/`rrcq`), identical in shape to the BB10 OS autoloader:

| rec | type | blocks | contents |
|---|---|---|---|
| 0 | `0x08` IFS | 160 (10 MiB) | QNX/BB10 IFS boot image (`FE 03 00 EA` at +0x190) |
| 1 | `0x09` RFS | 6147 | OS filesystem (QNX6FS `qnx6`, RCFS `rimh`) |
| 2 | `0x89` SIG2 | 1 | signature |
| 3 | `0x06` OS/MBR | 1 | MBR |
| 4 | `0x05` User | 6272 | user area |

…and it also embeds the **Android boot chain + system**:
- `app/aboot/aboot.c`, `bbss_wp_type`, `ANDROID-BOOT` @ ~547 MB (the Android
  LK `aboot`, production-signed, leaf `495b0ed3…`),
- `app_process` @ ~765 MB (the Android system's `app_process`).

So balika's conversion = the **BB10 autoloader container** carrying an
**Android boot chain + Android OS**. The **BlackBerry prototype**, by contrast,
uses the **Android GPT** layout (44 partitions: `sbl1/aboot/tz/rpm/boot/system/
vendor/modem/...`). To rebuild a flashable Android set from the prototype we
need the prototype's `boot`/`recovery`/`modem`/`data` (root/EDL) and the
boot-chain parts not in `backup_bootchain` (`hyp/pmic/devcfg/cmnlib/keymaster`).

## 3. Android prototype root path — the `diagnostics` service (CORRECTED in session 24)

> **Correction (session 24):** the "privileged file-read primitive" below was
> wrong. `FileTask` only checks existence and returns the path — it never reads
> file contents. A debug token **is** present, so the service is actually an
> **exec** primitive (`append_log` dtype 11 → `ExecTask` as uid 1301 ddt), not a
> read primitive, and SELinux blocks it from `/dev/block` and `/data/local/tmp`.
> See `session24-hardening-and-dirtycow.md` for the full re-analysis and the
> working Dirty COW (ptrace) primitive.

`com.blackberry.ddt.IDiagnosticService` is reachable from `shell`. Transaction
map (from `IDiagnosticService$Stub$Proxy`): `1=send 2=append 3=append_log
4=open 5=get_guid 6=get_sysvars 7=admin`.

Verified in session 24:
- `isTokenPresent()` is **true** (`bb_tokenserviced` running); token gates are
  open.
- `append_log(euid,dtype=11,params)` → `exec://params` → `SysTasks$ExecTask`
  → `Runtime.exec(params)` as `uid=1301(ddt)` in `u:r:diagnostics:s0`, with
  gids `system,log,shell,nvram,reset_cause`. Exfil via logcat
  (`/system/bin/log`); helper `tools/session24/dcx.sh`.
- `FileTask` is an existence check only; `send` returns an event ID, not an
  euid.
- SELinux denies `diagnostics` search on `/dev/block` and `/data/local/tmp`;
  `/nvram` is readable; no setuid binaries exist anywhere.

## 4. Artifacts
- This note.
- `recon/passport-android-dump/vendor/backup_bootchain/` (prototype boot chain +
  certs).
- Extracted certs: `recon/passport-autoloader-certs/` (retail boot0, prototype,
  balika).
- `session22-passport-android-prototype.md` (identity, dump, signing).

## 5. Safety
Read-only throughout. No flash / wipe / unlock. The prototype has no autoloader.
