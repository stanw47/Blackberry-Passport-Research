# Session 28 — Retail Passport BB10: RAM-loader boot-region attempt, the wipe rules, and re-root

Date: 2026-10-08. Device: retail Passport (Windermere EMEA, `QNX BLACKBERRY-E538`,
MSM8974AA, model_id `0x87002C0A`, BB10 10.3.x). Goal of the thread: explore a
software (no-desolder) write to the eMMC boot region via the host RAM-loader.

---

## 1. RAM-loader boot-region attempt — tooling established

- `bblink.py` (Blackberry-Classic-Research/tools; ported from bb10mt) with the
  OS-session reboot helper `session_reboot()` = Channel0(cmd=3).
- **Correct loader for this model is LDR_77**, not `loader_8D002C0A-00.bin`
  (previous sessions used the wrong-size sibling). Extracted from
  `tools/bb10mt/loaders.dat` and validated:
  - `loaders.dat` format: magic `BB10MTR\0`; entries = 6-byte name + 8×u32
    `[usize, 0, csize, 0, payload_offset, 0, crc, 6]` (stride 0x26).
  - LDR_77 @ table 0xb7c: usize `0x398b4`, csize `0x19831`,
    offset `0x5320ca`, crc `0xb53fca1e`.
  - **Payload compression is LZMA-alone (`5d 00 00 80`), NOT gzip**
    (`gzip.decompress` fails; `lzma.FORMAT_ALONE` works).
  - Result: `/tmp/opencode/loader_87002C0A-00.bin` — 235,700 B,
    magic@4 `0xD7D32D1F`, load addr@8 `0x0DD00000`, footer `0xD7C82D1F`.
- Host raw-USB access needs a udev rule (device node is root-only otherwise):
  `/etc/udev/rules.d/99-blackberry.rules` =
  `SUBSYSTEM=="usb", ATTR{idVendor}=="0fca", MODE="0660", GROUP="plugdev"`.
- Bring-up sequence reached but **not completed**:
  OS session `0fca:8017` → `session_reboot` (resp `04ff0000`) → BootROM
  `0fca:0001` → ping0 / get_var(2) → **model_id `87002C0A`** → `set_mode(1)`
  ACK → device re-enumerates → next write hits the stale handle → `ENODEV`
  while starting PasswordInfo.

## 2. THE WIPE RULES (confirmed; supersedes earlier assumptions)

1. **A failed / incomplete BootROM password handshake triggers a full device
   security wipe.** This is true **regardless of how BootROM was entered** —
   `session_reboot` from a live OS *or* the listener-first / powered-off entry.
   (User correction, 2026-10-08; consistent with session14 "Wipe on failed
   handshake".)
2. On the `Classic` test unit (session14) the same failure wiped the OS and left
   the device in the "Reload OS" loader; with the OS gone, SBL2 boots straight
   into it (BootROM `0x0001` is then no longer reachable by power-off+plug).
3. **The wipe re-runs on every USB host drop/error** while in the reload state
   (session14/20 — observed repeatedly). Keep the host link stable; kill all
   host listeners/pollers before touching the device.
4. Screen state observed: `www.bberror.com/bb10-0010` ("Reload OS" loader).
   `0015` seen once historically = corrupt filesystem / interrupted wipe
   (community; same meaning, same fix).

## 3. Recovery ritual (user-proven, repeatable)

1. Hold **Power + Volume Up + Volume Down** together (~30–40 s) to break out of
   the wipe/reload mode.
2. Run the official / pre-rooted autoloader (host polls first, then the device;
   keep USB stable). Whole reflash takes ~3 minutes on this unit.
   - Local asset: `/home/stanw47/priv-research/autoloaders/passport-rooted/`
     `Passport_10.3.03.3216_SQW100-1-2-3-4-root_v2.exe` (+ v2.1 radio `.signed`).
3. The 0010 loader speaks **official-updater protocol only** — bb10mt/bblink
   channel0 commands all time out against it (session14). Only the autoloader
   works.

## 4. Re-root after reflash (btool → __root)

- Root mechanism: `btool` (a `/bin/sh` script) runs **as root at boot**
  (autoroot via `/base/scripts/ota_info_pps.sh` symlink) and re-applies the
  pathtrust list with `/proc/boot/pathtrust !<path>` entries. Adding
  `!/base/bin/__root` makes the setuid `__root` shell usable.
- btool path: `/apps/sys.android.gYABgKAOw1czN6neiAT72SGO.ns/native/system/xbin/btool`
  (`/base/scripts/ota_info_pps.sh` symlinks to it). World-writable (+ACL) →
  patchable as devuser.
- Patch = insert `/proc/boot/pathtrust !/base/bin/__root` after the
  `mod_nvram` whitelist line (line 31). Patched file = 1932 B; host copies
  `/tmp/btool.patched` and repo `notes/session12-passport-root/btool.patched`
  are byte-identical; original = 1893 B.
- Apply: overwrite in place → **reboot** (only safe btool trigger = boot
  autoroot; switchzone changes OS slot and drops dev-mode/SSH) → test
  `echo "<cmd>" | /base/bin/__root`.
- **Dev Mode does not persist across reboots** — re-enable on device
  (Settings → Security and Privacy → Development Mode; device password
  `61482501`).
- SSH ritual each session: fresh 4096-bit RSA key; run
  `blackberry-connect 169.254.0.1 -password 61482501 -sshPublicKey /tmp/bb_key.pub`
  (tunnel must stay running); paramiko SSH to `devuser@169.254.0.1:22` with
  pubkey algs `rsa-sha2-512/256` disabled. Ports: 4455 qconnDoor / 22 sshd /
  5555 adbd.

## 5. Status / artifacts

- btool re-patched this session and verified remotely (`cmp` remote == patched);
  on-device backup at `/accounts/devuser/btool.orig.bak` (1893 B).
- **Re-root COMPLETE**: user rebooted (autoloader reflash done just before);
  autoroot ran the patched btool; `__root` verified uid-0 — wrote
  `/root/reroot_test` as `root:nto` (`echo "<cmd>" | /base/bin/__root`).
- Host artifacts: `/tmp/opencode/bb_bringup.py` (+ `bringup_log.txt`,
  `ramloader_probe/`), `loader_87002C0A-00.bin`, `bbssh.py`, `bbpush.py`,
  `/tmp/btool.patched`, `/tmp/btool.current`, `/tmp/btool.afterpush`.

## 6. Hard rules before any further boot-region work

1. **Never attempt a BootROM session until the handshake is proven to
   complete.** Current bblink fails because after `set_mode(1)` the device
   re-enumerates; the next command uses a stale handle → incomplete handshake →
   wipe.
2. Fix = re-open the device after every mode switch and complete PasswordInfo
   (HashPassV2) immediately; treat any USB error as abort-and-recover.
3. Validate the fixed flow on the Classic (sacrificial) first; do not park the
   Passport in the 0010/wipe-loop state.
4. Entry method does not reduce wipe risk; only a completed handshake does.
5. Next after re-root: retry the read-only RAM-loader probes (MCT `D9`,
   FlashRegions `B4`) and use the MCT Boot0 entry (kind `$2B`) to craft the
   boot-region F7 stream (see session15 §5).
6. On the BB10 Passport, **reboots and mode changes are performed by the user**
   (physical) on request — the unit is excessively hardened and host-triggered
   mode changes are what arm wipes.
