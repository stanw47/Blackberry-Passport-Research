============================================================
Passport -> Android  : contents of this folder
============================================================
SHA256SUMS.txt            - verify every file here against it
AUTOLOADER_GUIDE.md       - full build/flash documentation

1-FLASH-THIS-Passport_Android_balika_v2.exe
    Android autoloader: BB10 OS (ifs+rcfs+sig2+mbr) + radio +
    Android user GPT (new_user.img). Flash this AFTER boot0.
    sha256 66f4e587923e6f00e4397c8bca5a51c3...

2-RECOVER-BB10-root_v2.exe
    Stock pre-rooted autoloader. FLASH THIS FIRST to get back to
    a booting BB10 (it is already flashed - phone is recovered).
    Keep it: it is the only recovery path.
    sha256 19116fb750a1290150e471a043d8dc72...

boot0-step/                - the "boot0" (Android SBL) flash step
    boot0_flash.py         - probe-first flasher (this PC tool)
    bblink.py              - raw USB link library (used by it)
    boot0.img          - Android SBL image  (4 MiB boot0)
    loaders/loader_8D002C0A-00.bin - signed Passport RAM-loader

============================================================
HOW TO RUN THE boot0 STEP ON WINDOWS  (no probe, as decided)
============================================================
1. Install Python 3 on the Windows PC (python.org, check
   "Add to PATH"). Then open a Command Prompt in this folder and:

     pip install pyusb libusb-package

2. IMPORTANT order - the phone only enters BootROM while a host
   tool is already listening:
       a) start the command below FIRST
       b) hold the phone power button to POWER OFF fully
       c) plug the USB cable in
       d) the tool takes over and flashes boot0

   Command (from a CMD window that is in this folder):

     cd boot0-step
     python boot0_flash.py write --image boot0.img --yes

   (--yes skips the typing prompt; add --no-reboot if you prefer
    to control the reboot yourself.)

3. When it says it verified the read-back and reboots, the device
   is running Android SBL.

============================================================
THEN  (boot0 done)  : flash the Android OS
============================================================
4. On the Windows PC, run 1-FLASH-THIS-Passport_Android_balika_v2.exe
   (connect the phone in its "Reload OS / 0010" or BootROM state
   and follow cap.exe). This writes BB10 OS + radio + the Android
   user partition. Next boot = Android.

If anything goes wrong:
   - bb10-0015 screen = boot0 is still BB10 SBL; nothing is lost,
     just flash boot0 again with step 2.
   - To return to BB10 entirely, run 2-RECOVER-BB10-root_v2.exe.

============================================================
WARNING (read once, seriously)
============================================================
- An aborted/unknown-state BootROM handshake triggers a FULL
  DEVICE SECURITY WIPE. Never unplug or Ctrl-C during the
  handshake. The phone being recovered+stock now is the SAFE time
  to do this.
- You chose to skip the marker probe. The write is performed
  blind; boot0 raw-lane addressability has not been pre-verified.
  If it fails to verify read-back the exe/DOC tells you how to
  retry (bblink rawseq). Stock boot0 is untouched only until step
  4 of the flash actually commits.
- Use exactly this loader (loader_8D002C0A-00.bin). Dev/prod
  loader mixing rejects itself, but do not try other files.