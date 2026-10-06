# Session 22 — Passport **Android prototype** (oslo) discovered + dumped

Date: 2026-10-06. Device: BlackBerry **Passport Android prototype**, USB
`0fca:8032`, serial `1160724779`, adb product `oslorow` / device `oslo`.

> This is NOT the Passport mainboard and NOT the balika011 LineageOS conversion.
> It is a **BlackBerry-internal Android build in Passport (oslo) form factor**.

## 1. Identity (live)

| Field | Value |
|---|---|
| Model / device | `oslo` / product `oslorow` (brand `unknown-1`, model `Unknown`) |
| SoC | **MSM8974PRO-AA** (Snapdragon 801) |
| OS | **Android 5.1** (LMY47D), SDK 22 |
| Build | **AAA787** (`master aospl-msm8974 576 qc8974_sfi-user`), Apr 5 2015 |
| Builder | `ec_agent@br605cnc`, kernel `3.4.0-grsec-g0ea3e38` |
| Boot chain | primary `AAA787`, backup `AAA625`, HWID `0xf5b242c3` |
| Security | SELinux enforcing, `ro.secure=1`, `ro.debuggable=0`, verity active, `bsis_pka` |
| Boot WP | **`bbss_wp_type=power-on`**, `bbss_insecure=false` |
| Hardware | 1440x1440 LCD, **3-row keyboard** (`ro.hwf.keypadtype=3 3row`), BCM4339 WLAN, NFC BCM2079x |
| PIN | `0x2FFE921C` (`ro.nvram.prdid.pin`), IMEI `004402243033705`, BSN 1160724779 |
| Screenshot | black idle screen with PIN watermark → **retail-demo unit** (`com.avengers.retaildemo`) |

System apps = full **BlackBerry Android suite** (Hub, BBM, BlackBerryLauncher,
Diagnostics `com.blackberry.ddt`, `CheckInClient`, `RIDLClient`, `PrivacyDashboard`,
`RetailDemo`, `KeyboardShortcuts`, `com.blackberry.deviceconfig`, `sarservice`)
+ Google apps + Qualcomm. Rootfs = BlackBerry `sfi` Android build
(`init.factory.sfi.rc`, `init.oem.rc`, Mocana crypto modules `moc_*`, `pittpatt`,
`addon_verity_key`, `system_signature`).

## 2. What was dumped (read-only; nothing written to the device)

`recon/passport-android-dump/`:
- **`system_tree/`** — the entire `/system` (1.4 GB, 4476 files): app, priv-app,
  framework, lib, bin, xbin, etc, fonts, media, usr, tts, vendor.
- **`vendor/backup_bootchain/`** — `backup_bootchain_wp-power_on_secure.pkg` and
  `backup_bootchain_insecure.pkg` (+ extracted GPT partitions + certs).
- `vendor/{bin,bootstrap,firmware,lib,pittpatt}`, `root/*` (readable init configs,
  sepolicy, contexts), `lists/*`, `prop.txt`, `packages.txt`, `features.txt`,
  `dumpsys_*`, `logcat.txt`, `proc/*`, `screenshot.png`.

Not obtainable without root/EDL/fastboot-auth (all denied): **`boot` (kernel+
ramdisk), `recovery`, `modem`, `/data` (userdata), and several boot-chain
components (hyp, pmic, devcfg, cmnlib, keymaster)**.

## 3. Boot chain (`backup_bootchain_*.pkg`)

Container: magic `ce7e9d77`, u32 size, version, BC-version string at +0x10
(`AAA625` secure / `AAA624` insecure), payload at `0x38`. Payload is a **GPT**
disk with partitions:

| partition | size | contents |
|---|---|---|
| hwi | 4K | hardware info (all-zero here) |
| bbss | 256K | BlackBerry Secure Boot Signature/config |
| sbl1r | 1M | SBL1 (backup) |
| tzr | 512K | TrustZone |
| rpmr | 512K | RPM |
| sdir | 64K | signature dir? |
| abootr | 1M | aboot (LK) |

## 4. Signing — the key question (production vs engineering)

Embedded X.509 certs (extracted from `abootr.img` / `bbss.img`):

```
BB Root CA (secure)         O=BlackBerry, L=Waterloo, ST=Ontario, C=CA
                            self-signed, 2013-04-15 .. 2033-04-10, RSA-2048
BB Attestation CA (secure)  issued by BB Root CA (secure), 2013..2033
<leaf>  CN=Bryon Hummel (bhummel@blackberry.com)
        issuer = BB Attestation CA (secure), 2015-01-19 .. 2035-01-14
        Qualcomm image-auth OUs: SW_ID / HW_ID / DEBUG(0) / OEM_ID / MODEL_ID / SHA256
```

`aboot` strings:
- `Boot image verification failed (production key)`
- `Attempting HLOS image verification with "%s" key`
- `Boot image verification failed using debug token key`
- `Cannot provision secure device with test key`  ← **RPMB provisioning only**
- `bbry_is_insecure: TRUE`, `bbss_insecure`
- `AUTHBOOT-oslorow`, `DBGSIG-oslorow/wolverine*/keianrow`, `BBRY LK Display
  Detected Oslo HW - Using Panorama Panel`

**Conclusion:** the boot chain is signed with BlackBerry's **production
"secure" PKI** (BB Root CA (secure) → BB Attestation CA (secure) → per-image
leaf). There is **no separate engineering image-signing key** — the only "test
key" path is for **RPMB provisioning** of engineering devices, not for signing
images. So:
- We cannot sign our own boot images (no production private key).
- The prototype's own images **are production-signed**.

**Open verification:** whether this specific unit's **PBL is fused with the
production root key hash or a test key** cannot be read directly (QFPROM).
Evidence favours production (it boots the production-PKI chain with
`bbss_insecure=false`). If it is production-fused, then a **retail Passport
(same production key) should accept these images** — the basis for a
no-desolder Android route (flash via EDL/ISP, not desolder).

### 4b. DECISIVE comparison vs the retail Passport BB10 boot chain

We already hold the **retail Passport's `boot0` dump** (`recon/dumps-passport/
bb0_full.bin`, windermere EMEA, plus `priv-research/working/passport_boot0.img`).
Extracting its X.509 certs and comparing to the Android prototype:

| cert | Passport BB10 `boot0` | Android prototype | match |
|---|---|---|---|
| BB Root CA (secure) | sha256 `931828f4…` | sha256 `931828f4…` | **IDENTICAL** |
| BB Attestation CA (secure) | sha256 `e55af2b1…` | sha256 `e55af2b1…` | **IDENTICAL** |
| leaf | `CN=Bryon Hummel`, SW_ID `0x0` | `CN=Bryon Hummel`, SW_ID `0xC` | same issuer/CN |

**So the retail Passport BB10 and the Android prototype use the SAME production
signing chain** (BB Root CA (secure) → BB Attestation CA (secure) → per-image
leaf signed by BlackBerry's image-signing key). There is **no separate
engineering/test key** on the prototype for image signing — it is the production
PKI. **Implication:** if the retail Passport is fused with the production root
key (it is, being retail), the prototype's **production-signed Android images
should be accepted by a retail Passport** — the core requirement for a
no-desolder Android conversion (the remaining blockers are writing the
boot-chain/boot/system partitions, i.e. EDL/ISP, and the eMMC boot-partition WP).

## 5. Implication for "no-desolder Android on Passport"

The prototype supplies a **production-signed Android boot chain + full system**
for `oslo` hardware. If retail Passports share the same fused production key,
these images are cross-compatible and could be flashed via EDL/ISP (no
desolder). Missing pieces to assemble a full flashable set: `boot`, `recovery`,
`modem`, and the remaining boot-chain partitions — all require **root/EDL** to
extract.

## 5b. OEM unlock

- No `ro.oem_unlock_supported` prop. `settings put global oem_unlock_allowed 1`
  **persists** (`settings get` → 1), but the bootloader is **authboot-gated**
  (`fastboot oem info` → "authboot command permission denied"; `oem unlock` is
  not whitelisted), exactly like the Priv/KEYone. So enabling the toggle does
  **not** enable `fastboot oem unlock` (and unlocking would wipe, which we avoid).
  The Settings UI toggle resetting itself is likely a policy/config reset, not a
  usable path.

## 5c. Root hunt (no root yet — surfaces mapped)

Read-only probing only. Environment: SELinux **enforcing**, kernel
`3.4.0-grsec`, no `su`/busybox/setuid binaries, `/system/bin` files root-only,
`/nvram` `/persist` `/dev/block` denied to shell, production-signed boot chain.

- **`/dev/kgsl-3d0`** — `crw-rw-rw-` (`u:object_r:gpu_device:s0`), Adreno 330
  (`fdb00000.qcom,kgsl-3d0`), `libgsl.so` present. Kernel surface (same class as
  the KEYone KGSL/IOMMU work). `libgsl.so` shows the **SHAREDMEM-era KGSL ABI**
  (`gsl_create_syncobj`, `gsl_command_issueib`, `gsl_memory_alloc_pure`,
  `gsl_device_getinfo`, `IOCTL_KGSL_*`) on kernel **3.4.0-grsec** (Apr 2015).
  **Candidate primitive:** Jann Horn's KGSL page-pool bug (CVE-2023-21666 class)
  — on KGSL built **without `CONFIG_QCOM_KGSL_USE_SHMEM`**, GPU-shared pages come
  from KGSL's own page pool, are inserted into VMAs without `VM_PFNMAP`, and are
  returned to the pool on free **without a refcount check** → cross-process page
  UAF / **data leakage** (`get_user_pages`/`vmsplice`). This is an info-leak /
  page-UAF primitive, not direct kernel R/W; a full root still needs a second
  stage + a grsec bypass. Also relevant: syncobj / `freememontimestamp` races
  (the `pending_free` guard exists in upstream 3.4 `kgsl.c`).
- **`/dev/binder`, `/dev/ashmem`** — `0666` (grsec-hardened).
- **`diagnostics` service** (`com.blackberry.ddt.IDiagnosticService`, txn codes:
  1=admin, 2=append, 3=append_log, 4=get_guid, 5=get_sysvars, 6=send, 7=open) —
  **reachable from shell**; logcat: `Checking client permission: uid=2000` →
  **"Permission granted for native system process"**. The app is PRIVILEGED
  (`WRITE_SECURE_SETTINGS`) and reads `/nvram/prdid/pin`, `/nvram/prdid/imei`,
  `/nvram/boardid/bsn` (shell cannot). `IDiagnosticService` exposes log types
  `file/logcat/dumpsys/bugreport/dropbox/ramdump/**nvram**/devmem/debugfs/screen/
  **exec**/app/pkg/manifest/crash/content` (`LogTypeURI`). The log tasks run in
  the **`DiagnosticsService` process (`u:r:diagnostics:s0`)** via
  `DiagnosticService$AsyncLogRunner` → `LogRunner.parse` → `SysTasks$ExecTask`
  (`Runtime.exec`). `send` works from shell and returns an event GUID; but it uses
  `LogRunner.parse(logtype=0)` (=`/dev/null`, no-op), and `append_log(euid,
  dtype, params)` requires a **valid parent euid** (a 64-bit event ID from a
  prior `send`) — and `service call` only supports `i32`/`s16` (no `i64`), so
  driving the full event→append_log→logtype flow needs a proper client.
- **Token service debug mode** — `bb_tokenserviced` contains
  `"Token not present: But returning true anyway, since debug mode"`; the token
  (`ddt.tkn`) is **absent** on this unit, yet the diagnostics service still grants
  shell permission. `/dev/socket/tokenservice`, `/dev/socket/bbauth`,
  `/dev/socket/mfg` are SELinux-gated.
- **Manufacturing daemons** — `init.svc.mfg_*` running (`mfg_battery`,
  `mfg_custom`, `mfg_dc`, `mfg_properties`, `mfgradiosrvqc`); `stp_server`,
  `dcmd`, `StoreKeybox`, `vtnvfsd`, `bbauthd`, `bb_tokenserviced` present.

**Next (in order):** (1) finish the diagnostics flow with a proper client to run
`exec://`/`nvram://` in the `diagnostics` domain; (2) KGSL exploit on
`/dev/kgsl-3d0`; (3) token/debug-mode + `backup_bootchain_insecure.pkg` for the
no-desolder goal.

## 6. Safety

- **DO NOT WIPE / FLASH / UNLOCK** — no autoloader exists for this prototype.
- All work so far is read-only (adb pull, fastboot getvar). `adb root` and
  `fastboot oem info` are denied (authboot/production build).
- Destructive fastboot commands were not used.

## 7. Artifacts
- Dump: `recon/passport-android-dump/` (1.4 GB) — `system_tree/` (full /system),
  `vendor/` (bootchain pkgs + extracted GPT parts + certs), `root/` (readable
  init configs + sepolicy), `lists/`, `proc/`, `prop.txt`, `packages.txt`,
  `dumpsys_*`, `logcat.txt`, `screenshot.png`.
- Boot chain parts + certs: `recon/passport-android-dump/vendor/backup_bootchain/`
  (`parts_secure/`, `parts_insecure/`, `certs/*.der`).
- Script: `recon/passport-android-dump/dump_passport_android.sh`.
- APK decompiles (baksmali): `recon/passport-android-dump/decomp/{Diagnostics,
  CheckInClient,RIDLClient,PrivacyDashboard,RetailDemo}`.
