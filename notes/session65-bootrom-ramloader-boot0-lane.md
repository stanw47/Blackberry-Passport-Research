# Session 65 — The BootROM / RAM-loader boot0 lane (no-desolder boot-area write)

Date 2026-10-18. Follows session64 (SBL auth + BBSS boot-area load). Session64
concluded the unlock needs a **write to the eMMC boot area** (the insecure
BBSS/SBL/LK chain). This note documents the **existing no-desolder vehicle** for
that write: the BlackBerry **BootROM / RAM-loader raw lane**.

Sources: `F:\Passport-Android\README-FIRST.txt`, `AUTOLOADER_GUIDE.md`,
`boot0-step\boot0_flash.py`, `boot0-step\bblink.py`,
`boot0-step\loaders\loader_8D002C0A-00.bin`, `F:\tools\ramloadercommands.txt`,
`F:\SHOP-HANDOFF\*`.

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
## 3. Where this leaves the oslo prototype

Session64 established: the boot chain the device runs is the **boot-area** chain
(BBSS → SBL → LK), and the SBL/LK are authenticated — so the gate-free
engineering LK only boots under the **insecure** chain, which lives in boot0.
This lane is exactly how boot0 is written without desoldering.

Remaining oslo-specific unknowns (next):
1. **oslo boot0 image.** `new_boot0.img` is a *wolverine* (Passport) chain
   (`hwi product="wolverine"`; loader model `0x2C0A`). For oslo we must build the
   boot-area chain from the `F:\imggen` kit (`boot_gpt_insecure.bin` + `hwi` for
   oslo + `stage1/2/3` + `bbss` + `sbl1r` + `tzr/rpmr/sdir` + `abootr`) — i.e.
   what `tools/conversion/make_conversion_set.py` produces as `boot_insecure.img`.
2. **oslo RAM-loader.** The shipped loader is Passport-tuned. Need an oslo
   RAM-loader (or confirm the Passport loader's `IDtoADDR`/preflash family works
   for oslo). The PASSport loader uses `IDtoADDR(0x2C0A)`.
3. **`0x2019` (NV bits 42/43) on oslo** — present? (Believed part of the
   prototype's power-on freedom.) Must be confirmed read-only before any write.
4. **BootROM reachability on oslo**: prototype boots Android (stock OS), so the
   listener-first power-cycle ritual should reach PID `0x0001`. Confirm with the
   **read-only `preflight`** first (handshake + MCT print, no writes).
5. Keep `2-RECOVER-BB10-root_v2.exe` as the only recovery path; never abort a
   handshake.

--------------------------------------------------------------------------------
## 4. Recovery / failure modes

- **bb10-0015** = boot0 still BB10 SBL while the user area holds the Android GPT
  → "corrupt filesystem". Flash succeeded; recovery = any stock pre-rooted
  autoloader. Once boot0 is Android SBL, BB10 is never entered again.
- Aborted handshake → full security wipe; resident loader `0x8001` speaks only
  the official protocol (use the pre-rooted autoloader to recover).
