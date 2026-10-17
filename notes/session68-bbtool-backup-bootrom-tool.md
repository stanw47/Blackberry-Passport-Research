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

--------------------------------------------------------------------------------
## ADDENDUM — how to ENTER BootROM (the 5–10 s window + RED LED)

The tool's own logic (x86, `0x401630`): it checks **once** for the BootROM device
(`0FCA:0001`) at launch; if absent → `system("pause")` → `"Not in bootrom!"` →
`"Unplug usb!"` → exit. It does **not** poll. So the device must already be
enumerating as `0001` when the tool starts.

The entry behaviour is documented by the reverse-engineered **`ivoszbg/bb-usbdl`**:

> "On boot, the device checks whenever it's connected to a USB host. If it is,
> you get a **5 to 10 seconds gap** where it's waiting for a mode command, and if
> it doesn't receive any, the device continues with the boot flow. You'll know
> you're there when the **notification LED is red**. For testing, you can just
> disconnect the battery and unplug/replug the usb cable."

Boot flow (same project): `PBL → BBSS → BOOT0 (proprietary SBL1/2/3 replacement,
contains the RAMLoader restoration protocol) → STARTUP.BIN → IFS`. The
autoloader/BBTool both speak this RAMLoader protocol.

**Procedure to reach `0FCA:0001` on the oslo (Android) unit:**
1. Power the device **fully OFF** (no adb; hold power ~15 s).
2. Keep a USB host connected (the PC).
3. Power it on / plug it in → **within 5–10 s the LED goes RED** = BootROM
   `0FCA:0001` window (waiting for a mode command).
4. The host tool must send its mode command **inside that window** (BBTool's
   embedded ram-image; `bblink`'s `SetMode`), or the device continues booting.
5. **Never interrupt** the handshake (abort = full device security wipe).

So BBTool must be **launched while the device is in the LED-red window** (the tool
checks once) — or use our `bblink.py`, whose `connect()` handles the transition
(`reboot any non-BootROM product → connect PID 1 → SetMode(1)`) more robustly than
the single-shot exe. Our `loader_8D002C0A-00.bin` (Passport RAM loader) is on disk
at `F:\Passport-Android\boot0-step\loaders\`.
