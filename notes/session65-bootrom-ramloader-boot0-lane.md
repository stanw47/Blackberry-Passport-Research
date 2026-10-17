# Session 65 — The BootROM / RAM-loader boot0 lane (no-desolder boot-area write)

Date 2026-10-18. Follows session64 (SBL auth + BBSS boot-area load). Session64
concluded the unlock needs a **write to the eMMC boot area** (the insecure
BBSS/SBL/LK chain). This note documents the **existing no-desolder vehicle** for
that write: the BlackBerry **BootROM / RAM-loader raw lane**.

Sources: `F:\Passport-Android\README-FIRST.txt`, `AUTOLOADER_GUIDE.md`,
`boot0-step\boot0_flash.py`, `boot0-step\bblink.py`,
`boot0-step\loaders\loader_8D002C0A-00.bin`, `F:\tools\ramloadercommands.txt`,
`F:\SHOP-HANDOFF\*`.

> **SCOPE CORRECTION (2026-10-18).** This lane is the **BB10-Passport → Android
> ("balika") conversion** tool. It is **not** the route to put LineageOS 18.1 on
> an already-Android prototype (oslo). Its *mechanism* (a BootROM raw write to the
> hardware boot0 MMC partition) is a generic no-desolder **boot-area write
> vehicle**, but its *payload* (`new_boot0.img`, a **wolverine** boot chain) and
> its *loader* (Passport model `0x2C0A`) are wrong for oslo, and the oslo goal is
> a **custom recovery + ROM**, not a BB10→Android EVM swap. Treat §1 only as "the
> raw boot0-write machinery exists and how it works" — the oslo→LOS route is a
> separate question (see §3, rewritten).

--------------------------------------------------------------------------------
## 0. TL;DR

- **The lane exists and is proven:** `boot0_flash.py` + `bblink.py` talk to the
  device's **BootROM** over USB (pyusb), load a signed **RAM-loader**
  (`loader_8D002C0A-00.bin`), and write the 4 MiB **hardware boot0 MMC
  partition** (= the boot chain BBSS loads, per session64) via the loader's raw
  flash lane. **No desolder.**
- **Gate:** the device's own **"0x2019" NVRAM unlock** (NV bits 42/43 = byte 5
  `0x0C`), already set and persisted.
- **What it writes:** `new_boot0.img` (3 896 320 B, SHA256 `28a82c86…`) = the
  Android/insecure boot chain: **GPT @ LBA1, `hwi` product="wolverine",
  stage1/2 (magic `D1DC4B84`), stage3 (ELF), bbss, sbl1r, tz/rpm/sdi, abootr**
  (fits 4 MiB). This is the boot-chain form of the `F:\imggen` kit
  (`boot_gpt_insecure.bin` + stage1/2/3 + bbss + sbl1r + tzr/rpmr/sdir + abootr).
- **Two-step conversion:** (1) boot0 step (`boot0_flash.py write`) → Android SBL
  active; (2) user step (`Passport_Android_balika_v2.exe`) → Android OS + user
  GPT (`new_user.img`). BB10 never boots after step 1.
- **Risk:** an aborted/unknown BootROM password handshake = **full DEVICE
  SECURITY WIPE**. The RAM-loader may not have a Passport-native path for the
  **hardware** boot0 (probe first); recover BB10 with `2-RECOVER-BB10-root_v2.exe`.

--------------------------------------------------------------------------------
## 1. The raw-lane tool (`boot0_flash.py`)

- Loader: `loaders/loader_8D002C0A-00.bin` (222 624 B; load addr `0x0DD00000`,
  magic @4 `D7D32D1F`, footer @-8 `D7C82D1F`). Model `EXPECT_MODEL=0x87002C0A`
  (Passport windermere EMEA), `PREFLASH=0x40` (Passport family).
- **BootROM entry ritual:** the device only enumerates as **BootROM PID `0x0001`**
  while a host tool is already listening → start the tool, **power the device
  fully OFF**, plug USB in. PID `0x8001` = resident "Reload OS" loader (official
  protocol only — recover BB10 first); `0x8017` = OS session.
- Verbs: `selftest`, `verify-image`, `preflight` (read-only: handshake, flash
  regions, MCT), `probe` (non-committing marker), `write` (full commit).
- `write` streams `new_boot0.img` in `BLOCK=0x3FF4`-byte **F7** writes into the
  boot0 block range parsed from the **MCT** (record kind `$2B` "Boot0 MMC":
  `start,end,flags`), then sends a **560-byte "install seal"** (`bb.dummy_sig()`),
  then **Complete**, then CREAD read-back, then a loader reboot.

### Ramloader ops used (`F:\tools\ramloadercommands.txt`)
`20` persistent data · `B1` flash IDs · `B4` flash regions · `B5` flash info ·
`C0 40 COMPLETE` · `C4 READVERIFY` · `D7 SetActiveMCT` · `D9 Retrieving MCT` ·
`E4 CREAD INIT` / `E5 CREAD (<=0x3FA0)` · `EE ERASE_SECTOR` · `EF 80 Reboot` ·
`F7/F8 Write data` · `F9 40 SIGNATURE_TRAILER`. Wipe-class ops that exist and
must be avoided: `C6 FACTORY_WIPE`, `C8 GRS_WIPE`, `D5 SUPER_NUKE`,
`E8 WIPE_SECURITY`.

### Probe (non-committing, the safety valve)
`F7` write of `CLSCMRKR`+8 bytes into boot0 **dead slack at byte 2048*512**
(sector 2048, unexecuted, boot chain untouched) + `CREAD` read-back. The full
write is only allowed if the marker reads back (or `--force`). `README-FIRST.txt`
records that on the Passport the operator chose to **skip the probe** ("no
probe, as decided") — the lane is used blind.

--------------------------------------------------------------------------------
## 2. The official-updater SBL1 lane (alternative, research-pending)

`AUTOLOADER_GUIDE.md` §6: `cap.exe` (the official updater core) contains
`BootromFileImage` / `BootchainFileImage`, `MCT_BOOT0_MMC` / `MCT_BOOT1_MMC`
region handling, and a `SWITCH_BOOT` ("Switch Boot Partition…") — i.e. an
**official SBL1-update path** the `0x2019` unlock is meant to enable, possibly
**no-wipe and autoloader-carried**. Header format still buried in virtualized
C++ (`FUN_00432460` → `BootromFileImage::LabelFileImage`); needs a focused
Ghidra pass + on-device confirmation. `boot0_flash.py` is the working vehicle
today.

--------------------------------------------------------------------------------
## 3. The oslo → LineageOS 18.1 problem (this lane is NOT the answer by itself)

oslo is an **Android prototype** (LMY47D AAA787), not a BB10 unit, so the balika
conversion does not apply. What the device actually needs:

- **A custom recovery / ROM won't boot while locked.** Session63 proved the LK
  authenticates the HLOS (`boot.img`/`recovery.img`) with the production key
  (`AUTHBOOT-oslorow`) and dm-verity covers `/system`. So a LOS recovery/boot.img
  flash is rejected at boot.
- **Session64 added:** the LK is itself authenticated by the SBL (loaded from the
  boot area). The gate-free engineering LK has *no* HLOS gate — but it can only be
  *loaded* under the insecure chain (boot0). So a boot-area write is required
  **iff** the engineering LK is the chosen route.
- **The real fork is how the LK stops verifying the HLOS:**
  1. **`is_unlocked` state** — the LK skips HLOS verification when unlocked
     (session62). If a fastboot / OEM / factory unlock can set it, a custom
     recovery flashes and **boots with no boot-area write at all**. Highest
     priority, and purely read-only to investigate.
  2. **Insecure chain + engineering LK** via a boot-area write — needs an **oslo**
     boot chain and an **oslo** RAM-loader, neither of which the balika kit
     supplies (`new_boot0.img` is wolverine; the loader is `0x2C0A`). This is the
     *fallback*, not the first move.

Unknowns to resolve before any device write (all read-only):
1. **LK unlock path** — is there a `fastboot oem unlock` / `flashing unlock`, an
   `is_unlocked` NV/BCB flag, or a factory unlock? (session62's open thread.)
2. **Which area the LK boots from** — the device has both a user-area `aboot`
   (4 MiB, `block_device`, vold-writable) and a boot-area `abootr`. Resolve the
   §64.3 ambiguity: if the running LK is the **user-area `aboot`**, an OS-only LK
   swap is conceivable (still subject to SBL auth).
3. **oslo boot chain + loader** (only if (1) fails): build a
   `boot_insecure.img`-style payload from the `F:\imggen` kit and obtain an oslo
   RAM-loader.
4. Never abort a BootROM handshake (full device security wipe).

--------------------------------------------------------------------------------
## 4. Recovery / failure modes

- **bb10-0015** = boot0 still BB10 SBL while the user area holds the Android GPT
  → "corrupt filesystem". Flash succeeded; recovery = any stock pre-rooted
  autoloader. Once boot0 is Android SBL, BB10 is never entered again.
- Aborted handshake → full security wipe; resident loader `0x8001` speaks only
  the official protocol (use the pre-rooted autoloader to recover).
