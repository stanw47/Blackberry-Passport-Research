# Session 29 — Spec: user-area-boot experiment (Android on the retail Passport
# without touching the eMMC boot partitions)

Date: 2026-10-08. Device: retail Passport (Windermere EMEA, MSM8974AA,
model_id `0x87002C0A`, BB10, uid-0 root). Follows session 28 (wipe/re-root).
Goal: determine whether the PBL can boot the Android chain from the **writable
user area**, so that `boot0`/`boot1` (eMMC boot partitions, hardware-WP) never
have to be written.

---

## 0. Hypothesis and the two competing models

**Model A — user-area boot.** The PBL loads the first-stage chain from the eMMC
**user area** (selected by `ext_csd[179]` `BOOT_PARTITION_ENABLE`), the standard
Qualcomm configuration. Then everything Android needs (SBL1/aboot/rpm/tz/sdi +
boot/recovery/system/userdata) is in writable storage, installed via the signed
official updater; boot0/boot1 stay stock and untouched.

**Model B — boot-partition boot.** The PBL is fused/configured to boot only from
the eMMC boot partition (`boot0`). Then the chain head
(`stage1/2/3 + bbss + sbl1r ...`) must be written to `boot0` and the WP is a
hard wall (loader/hardware only).

**Evidence weighing toward B (new this session):** the prototype's
`backup_bootchain_*.pkg` payload is itself a **boot-partition GPT** carrying
`hwi, bbss, sbl1r, tzr, rpmr, sdir, abootr` — i.e. this device family keeps its
(backup) chain in the eMMC boot partition. The experiment below is still the
cheapest way to falsify/confirm A.

## 1. Ground truth we already have

- `ext_csd` **retail Passport, LIVE READ 2026-10-08** (`tools/session29-extcsd`,
  root via `__root`; same values read from `/dev/emmc{,/boot0,/boot1,/uda0,
  /user0,/os0,/cal_work0,/dmi0,/nvram0}`):
  `[162] B_BOOT_INFO = 0x02`, `[168] ERASE_GRP_SIZE = 0x20`,
  `[170] BOOT_CONFIG_PROT = 0x00` (**not fused**),
  `[173] BOOT_WP = 0x04` = **B_PWR_WP_EN (power-on / temporary)** — the
  session18/20 "permanent" reading was a bit-numbering error (bit2 is PWR, not
  bit0 PERM), `[174] BOOT_WP_STATUS = 0x0a`, `[177] BOOT_BUS_WIDTH = 0x04`,
  `[179] PARTITION_CONFIG = 0x48` (BPE=1 → boot0; bit6 set on this eMMC —
  the Classic read `0x08`).
- **WRITE_PROTECT CLR via the stock driver returns EIO and does not change
  `[173]`** (rc=5 for mode 0 and 1, nlba=8192, on `/dev/emmc/boot0`) — same
  driver-side wall as the Classic. Clearing still needs a CMD6 the stock driver
  cannot emit.
- `DCMD_MMCSD_CARD_REGISTER = 0xC0181A14` (24-byte struct
  `action,type,address,length,rsvd[2]` + 512-byte ext_csd) — reachable only via
  the `g_Disk_Drivers` group channel / root tooling, not plain devuser.
- Official updater records map types → **writable** partitions only
  (`.ifs/.rcfs/.mbr/.ufs/.nvram/...`); no record type maps to boot0/boot1
  (session15).
- **Balika `v2.0-android.signed`** (824 MB, `priv-research/autoloaders/
  passport-android/`): IFS/RCFS/MBR records are **byte-identical** to the stock
  pre-rooted BB10 autoloader; only the UFS record differs. Its UFS payload
  begins with the exact imggen `user_gpt.bin` layout and carries real
  user-area images (SBL1 magic `D1 DC 4B 84`, aboot, RPM/TZ ELF, sdi, modem).
  Record map: IFS `0x188` (10 MB), RCFS `0xA00188` (402,849,792 B),
  SIG2 `0x18A30188` (64 K), MBR `0x18A40188` (64 K),
  **UFS `0x18A50188` (411,041,792 B)**.
- Production PKI is shared (BB Root/Attestation CA (secure), session22) → the
  retail fuse will cryptographically accept the prototype/balika images.

## 2. `ext_csd[179]` PARTITION_CONFIG — exact values

| bits | field | value | meaning |
|---|---|---|---|
| 7 | BOOT_ACK | 0/1 | boot acknowledge from boot partition |
| 5:3 | BOOT_PARTITION_ENABLE | 0 | not enabled (PBL may fall through / user area) |
| | | 1 | **boot0** (current: `0x08`) |
| | | 2 | boot1 |
| | | 7 | **user area** |
| 1:0 | PARTITION_ACCESS | 0 | host data access = user area (keep 0) |

Candidate values to test (write via CMD6 SWITCH; `BOOT_CONFIG_PROT = 0x00`, so
not fused):
- `0x00` — BPE=0 (PBL fallback behaviour unknown; may try user area or SD)
- `0x08` — BPE=1 boot0 (control/current)
- `0x38` — BPE=7 user area (JEDEC)
- `0xB8` — user area + BOOT_ACK
Keep PARTITION_ACCESS = 0 throughout (`mmc bootpart enable 7 0` ≈ 0x38).

**How to write it (the critical dependency, ranked):**
1. Official updater/autoloader path — analyze whether flashing the MBR/UFS
   records touches PARTITION_CONFIG (Balika set it by programmer on a blank
   eMMC, so probably not — confirm).
2. RAM-loader `DE BOOT_MODE` (`0xDE`) probe — deferred until the
   autoloader-compilation step (user rule: BootROM work only via autoloader).
3. Hardware programmer (UFI/EasyJTAG) — out of scope here (touches WP).

From the running BB10 OS **no** CMD6 path exists (sdmmc driver has no raw
passthrough — sessions 7p/9a/10a).

## 3. What goes where (exact)

### 3a. User area (writable) — Android conversion layout
Offsets are relative to the user image base = **eMMC `0x200000`** (imggen
convention). Install via the official updater (balika UFS record), or
manually.

| partition | img offset | size | notes |
|---|---|---|---|
| gsign | `0x4400` | 3 K | |
| mct_b | `0x5000` | 4 K | backup MCT |
| pad | `0x6000` | 40 K | |
| nvram | `0x10000` | 4.1 M | device nvram |
| calwork_b | `0x400000` | 28 M | from device |
| dmi_b | `0x2000000` | 32 M | |
| calback_b | `0x4000000` | 64 M | from device |
| **aboot** | `0x8000000` | 4 M | LK (MBN/hdr, not plain ELF) |
| **sbl1** | `0x8400000` | 1 M | Qualcomm SBL1 (magic `D1DC4B84`) |
| **rpm** | `0x8500000` | 512 K | ELF |
| **tz** | `0x8580000` | 512 K | ELF |
| **sdi** | `0x8600000` | 512 K | |
| fsc/modemst1/modemst2 | `0x8800000`.. | 1.5 M ea | |
| blog/perm/nvuser/ddr/bkup_ddr | `0x8c80000`.. | | |
| prdid(+sig)/boardid(+sig) | `0x9000000`/`0x9040000` | | |
| fsg | `0x9080000` | 1.5 M | |
| ssd/metadata/frp | `0x9800000`.. | 1 M ea | |
| bcota | `0xa000000` | 4 M | backup-chain staging (proto mechanism) |
| rcause/spare/persist/crypto | `0xa400000`.. | | |
| boot | `0x10000000` | 32 M | filled later (recovery/sideload) |
| recovery | `0x12000000` | 32 M | filled later |
| modem | `0x14000000` | 75 M | `NON-HLOS.bin` (FAT) |
| system | `0x18800000` | 2.5 G | filled later |
| cache | `0xb8800000` | 1 G | |
| userdata | `0xf8800000` | | |

imggen `new_user_gpt.bin` (local, /tmp/opencode) contains everything up to
`boardid` (`0x9080000`); boot/recovery/modem/system are installed afterwards
by the recovery/sideload step.

### 3b. Boot partition (WP — Model B only, informational)
imggen `new_boot1.img` (3,896,320 B) GPT:

| partition | offset | size |
|---|---|---|
| hwi | `0x4400` | 4 K |
| stage1 | `0x5400` | 128 K |
| stage2 | `0x25400` | 320 K |
| stage3 | `0x75400` | 8 K |
| bbss | `0x77400` | 128 K |
| sbl1r | `0x97400` | 320 K |
| tzr | `0xe7400` | 512 K |
| rpmr | `0x167400` | 256 K |
| sdir | `0x1a7400` | 64 K |
| abootr | `0x1b7400` | 2 M |

## 4. Experiment phases

**Phase 0 — ground truth (non-destructive). ✅ DONE 2026-10-08.**
1. ✅ `ext_csd` read live on the retail (values above). Key: `[170]=0x00`,
   `[173]=0x04` → boot-partition WP is **power-on only, not fused**.
   Tool + recipe: `tools/session29-extcsd/` (`extcsd_probe.c`, `start_min.S`,
   `stub.s`, `build.sh`). Deployed as `/accounts/devuser/extcsd_probe`,
   pathtrust-whitelisted, run via `__root`.
2. Establish where the PBL would read SBL1 from in the user area:
   - the user GPT's `sbl1` partition is at img `0x8400000` (eMMC `0x8600000`);
   - compare with the MBR record (`0x18A40188`) → partition entry
     `LBA_first=0x6000, count=0xFA000` (sector 0x6000 = 12 MB) — TBD whether
     the PBL uses the MBR, the GPT, or a fixed offset;
   - if the prototype's ext_csd can be read (needs root/diag — currently
     blocked), its `[179]` would settle Model A vs B immediately.

**Phase 1 — stage the Android user area (destructive to BB10 user area).**
- Preferred: run the balika package through the updater (Windows autoloader
  `.exe` or equivalent), after `mod_nvram -d` (delete downgrade blocklist) on
  the rooted device.
- The stock IFS/RCFS/MBR records are identical to stock → only the user area
  changes; boot0/boot1 untouched.
- Recovery: reflash stock/root_v2 autoloader (user area restored).

**Phase 2 — switch the boot source (the gate).**
- Set `ext_csd[179]` to `0x38` (user area), or `0x00` (not enabled) as the
  alternate candidate, via whichever channel is available (see §2).

**Phase 3 — power cycle and observe (user performs the reboot).**
- BootROM USB `0fca:0001`? EDL `05c6:9008`? fastboot (screen/LED)? nothing?
- `lsusb` signature + screen tells us which model is true.

**Phase 4 — if fastboot comes up (Model A holds).**
- `fastboot flash recovery recovery.img` (balika LineageOS recovery);
  Vol-Up+Power → yellow LED → recovery → factory reset/format data →
  apply update from ADB → `adb sideload lineage-18.1-20250124-UNOFFICIAL-venice.zip`
  (local at `priv-research/balika/`).
- Then flash `boot.img`/`system.img` for the full OS per the balika flow.

**Rollback for every phase:** 3-button reset (Power+VolUp+VolDn ~30–40 s) →
autoloader reflash (≈3 min); root restored via session28 procedure.

## 5. Open questions to resolve before Phase 2

1. Does the MSM8974 PBL on this fuse/config honour `BOOT_PARTITION_ENABLE=7`
   (or 0) and read the user area — and from which offset (MBR entry vs GPT
   `sbl1` vs fixed LBA)?
2. Does the official updater/autoloader already set PARTITION_CONFIG when
   flashing (would remove the Phase-2 dependency)?
3. Will the retail updater accept the balika package (hwid/version/rollback
   checks) after `mod_nvram -d`?
4. Is the user-GPT `sbl1` image the first stage the PBL wants, or must the
   `stage1/2/3 + bbss` head also exist in the user area? (Android-standard
   devices boot with plain SBL1; this family may differ.)

## 6. Risks / notes

- Phase 1 destroys the BB10 install (user area) — the device will not boot
  BB10 afterwards; only proceed when a reflash is acceptable.
- boot0/boot1 are never touched by this spec; eMMC WP stays as-is.
- All reboots/mode changes on the BB10 Passport are performed by the user
  (session28 rule); the 3-button reset + 3-minute reflash is the undo.
- If Model B is confirmed (PBL boot-partition only), the no-WP route is closed
  and Android requires the boot0 head to be written — i.e. the
  autoloader-compilation boot-region write (session28 §6) or hardware.

## 7. Artifacts

- **`tools/session29-extcsd/`** — working BB10/QNX userland toolchain +
  `extcsd_probe` (reads ext_csd; attempts WRITE_PROTECT CLR). Build recipe
  proven this date (stub libc SONAME + minimal crt + `-z now`); see `build.sh`
  header for the three pitfalls that broke earlier builds.
- Balika package: `priv-research/autoloaders/passport-android/
  v2.0-android.signed` (UFS payload @ `0x18A50188`, 411,041,792 B) and
  `Passport_Android_balika_v2.exe`.
- imggen outputs: `/tmp/opencode/new_boot1.img`, `/tmp/opencode/
  new_user_gpt.bin`; templates in `priv-research/imggen/files/`.
- LineageOS: `priv-research/balika/lineage-18.1-20250124-UNOFFICIAL-venice.zip`.
- ext_csd recipe: session9a (g_Disk_Drivers + berrycore python, DCMD
  `0xC0181A14`).
- Recovery autoloaders: `priv-research/autoloaders/passport-rooted/`.
