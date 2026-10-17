# Session 68 — `BBTool_backup.exe`: a BootROM flash-backup tool (msm8974 RAM image)

Date 2026-10-18. File: `%USERPROFILE%\Downloads\BBTool_backup.exe`
(266 240 B, sha256 `bf66fb8fb775fbb86640d1b37f4a263fbe7faf98b0921cc76e375e07ada76cec`).

## What it is
A **Windows x86 PE32** host tool (5 sections, MSVC / MSVCP140), built from
`C:\Users\user\source\repos\BBTool\Release\BBTool.pdb` (a third-party "BBTool"
project; version resource says *Research In Motion / BlackBerry*). Imports
`SETUPAPI.dll` (USB enumeration) + `KERNEL32`/`ADVAPI32`.

It drives the **BlackBerry BootROM → RAM-loader** lane:

1. **BootROM detect** — device path `\\?\usb#vid_0fca&pid_0001#`,
   `"BootRom detected! Sending ram-image..."`.
2. **Upload embedded RAM image** — `"Ram-image sent. Waiting for 20 seconds for it
   to appear."` → RAM-loader `\\?\usb#vid_0fca&pid_8001#`,
   `"Ram-image detected!"` / `"Ram-image loading failed!"` / `"Not in bootrom!"` /
   `"Unplug usb!"`.
3. **BootROM handshake** — `"Set mode failed!"`, `"Get Password Info failed!"`,
   `"Not ready!"` (the same handshake that wipes the device if aborted).
4. **Read the flash → `dump.bin`** —
   `"Backing up the flash content."`, output `dump.bin`, progress `"%x of %x"`,
   `"Backup done."`, `"Failed to open/read/write! %i"`,
   `"Invalid response size/crc!"`.

## The embedded RAM image (device-side loader)
The tool embeds a full msm8974-class RAM loader. Its strings (in `.rdata`):
- **msm8974 / RPMB**: `8974: first RPMB key invalid, try again`,
  `RPMB key failed validation`, `Backing up RPMB %d to NV %d`,
  `Error retrieving RPMB Record 0x%X`.
- **Flash**: `Flash erase failure`, `Flash write failure`, `BlockReadEx() failure`,
  `PageReadEx/MultiPageReadEx/PartialPageReadEx: flash read failed`,
  `MCT not found!`, `MCT does not exist at 0x%X!`, `Could not retrieve MCT table`,
  `MCT too new (0x%X)`, `Erase of the MCT failed`, `failed to update LAL`.
- **Health**: SAMSUNG/MICRON/TOSHIBA/SANDISK/Hynix MMC smart reports, wear counts.
- **TrustZone**: `TZ DisableSecureHwAccess() call returned value = %s and status = %d`.
- **Identity**: `reason=ramimage_reset$creatorid=ramimage$`,
  `QC NHLOS (Preload Images) Info:` + `QC_IMAGE_VERSION_STRING=`.
- Misc: `Boot count exceeded!!!`, `OS not found or invalid OS present`,
  `Password record not found`, `Flash Failure, Password`,
  `Unknown signature primitive!`, `GetEncryptionChallenge: RSA ...`.

## Why it matters here
- It is a **no-desolder BootROM READ lane** (same family as `boot0_flash.py`), but
  with a genuine, device-side RAM loader that supports **block read *and*
  erase/write** (write strings present) as well as **MCT** access.
- **Open use #1 — settle the LK load-area question:** if this RAM-image can read
  the **hardware boot0** region (MCT kind `$2B`), running a backup would finally
  let us read the boot-area chain (`sbl1r`/`abootr`) and confirm/deny session67's
  conclusion without desoldering.
- **Open use #2 — boot-area WRITE vehicle:** the RAM-image's erase/write + MCT
  paths may allow writing the boot area (insecure chain + engineering LK), i.e. a
  controllable version of the `boot0_flash.py` lane.
- **Risk:** enters the BootROM handshake — an aborted/unknown handshake = **full
  device security wipe** (same rule as the other BootROM tools). Probe/read-only
  first; device must be in BootROM (listener-first, powered off then plugged).

## Next
1. Extract/serve the embedded RAM image and identify its exact upload format
   (the tool's USB framing) so we can reuse the RAM image (read the boot area, or
   extend to write) exactly as `boot0_flash.py` does for the Passport.
2. Confirm the flash range the tool backs up (whole user area? MCT-driven?) and
   whether `boot0` (MCT `$2B`) is included.
3. Decide with the user whether to run it read-only against the attached oslo.
