# BlackBerry Passport (SQW100) — Research

> Root, boot-chain, driver-forensics, and no-desolder unlock research on the
> BlackBerry **Passport** (BB10 / QNX, MSM8974AA).
>
> Part of the **[Blackberry-Research](https://github.com/stanw47/Blackberry-Research)**
> collection · [Williamson Security Solutions](https://williamsonsecuritysolutions.com)

---

## Disclaimer

> **Research aid, not a flashing guide.** Editing eMMC boot partitions or
> toggling write-protect can **permanently brick** the device. This Passport is
> currently non-bootable — recovery is the priority. For educational / defensive
> research on a device the author owns. **At your own risk.**

---

## Device Details

| Field | Value |
|---|---|
| Model | BlackBerry Passport **SQW100-1** |
| Codename | `passport` / `windermere` |
| SoC | Qualcomm **MSM8974AA** (Snapdragon 801) |
| OS / software | **BB10 / QNX 10.3.3.3216** (same OS as the Classic), `BLACKBERRY-603C`, WindermereEMEA |
| Current build | **10.3.3.3216** (rooted autoloader) |
| Previous builds | stock 10.3.3 |
| Carrier / unlock | carrier-unlocked; bootloader locked; boot-partition WP permanent |
| SIM | single |

---

## Current Status

The Passport was **rooted (uid-0)** and bootable, but after a raw `CMD6`
experiment it entered a **permanent red-blink loop** (`11011`, "Flash Erase
Failure") and is now **non-bootable**. Autoloader recovery fails at ~13%
("Signature Trailer"). Two independent root-cause findings:

- The **erase failure is a policy failure**: an armed security wipe tries to
  erase the boot region against a **fused card-level write-protect**
  (`BOOT_WP[173] = 0x04`, permanent), so BootROM can never complete the erase.
- Separately, **`FS_DIRTY_ALL` is RPMB-backed**, so even a perfect chip-off
  restore of `boot0`/`boot1`/`user` may **not** clear it.

The device's real strength remains that **MSM8974AA is the exact `imggen`
target**, so a no-desolder Android conversion is **mapped but not yet proven**.

**A replacement main board is on the way.** Once it arrives, the Passport will be
returned to service on the new board, and the current (bricked) board becomes a
**dedicated test unit** — experimentation on it will follow (recovery attempts,
UART capture, chip-off/chip-replacement trials, and the `imggen`/RAM-loader
Android path), without risking the working device.

---

## Completed

- **Root** (previously) via the getroot/pathtrust ritual.
- **Driver forensics:** the MMC WP gate (`0xF182`), WP handler (`0x108D0`), and
  the per-open-node `ext` model fully decoded; CID is live-read; no ext_csd cache.
- **eMMC dumps:** `boot0`, `boot1`, `os0` recovered.
- **`FS_DIRTY_ALL` forensics:** shown to be RPMB-backed.
- **`imggen` / `passport_stage3`** decoded (prototype bootloader + PBL debug path).

## Achieved

- **Real uid-0 root** (previously) via the getroot/pathtrust ritual.
- **eMMC dumps recovered**: `boot0` (SBL1), `boot1` (blank), `os0` (QNX IFS).
- **Full driver write-protect model decoded** (gate `0xF182`, handler `0x108D0`,
  per-open-node `ext`; CID live-read; no ext_csd cache).
- **`FS_DIRTY_ALL` root-caused to RPMB** — a decisive negative for chip-off restore.
- **Install-seal (`F9 40`) contract root-caused** — explains the red-blink vs `cap.exe`.
- **`imggen` boot0/user images generated and validated offline** (not flashed).

## In Progress

- **Device recovery.** The Passport is parked in the `11011` state until direct
  eMMC access. Highest-value next step: **UART capture** of the FS_DIRTY/Nuke
  sequence.

## Failed

- **`rimboot_update` software WP bypass** — its own gate aborts; the NOP-bypass
  fails `EROFS`; forcing the branch arms NV bits but `BOOT_WP` stays `0x04`.
- **NV power-cycle ritual** — decisive negative; WP remains permanent.
- **Autoloader recovery** — fails at the Signature Trailer (~13%).
- **`/dev/mem`** — a decoy (uniform `0xdeadbeef`, no persistence).

## Future Plans

1. **Fit the incoming replacement main board** and return the Passport to service.
2. On the old (bricked) board: locate **UART test points** and capture the live
   FS_DIRTY/Nuke sequence.
3. Compare **chip-off restore vs full eMMC chip replacement** (a blank chip
   re-provisions RPMB).
4. Once a board boots: the no-desolder `imggen`/RAM-loader Android conversion.

---

## Community Activity

- **balika011 (Balazs Triszka)** ported **LineageOS 18.1 (Android 11)** to the
  Passport by swapping the eMMC and reflashing `boot0`/`boot1`; **Guizmox**
  helped stabilise it. Prototype Passports have an **unlocked bootloader** (no
  desoldering needed). An eMMC compatibility list is maintained at balika011.hu.
- **Guizmox** documented the Passport **Android prototypes** (builds AAA249 ...
  AAC014, Android 4.4-5.1) that made the port possible.
- **Zinwa P26** - a 2026 DIY kit replacing the mainboard with a Helio G99,
  12 GB RAM, 256 GB and Android 14 (kit only; an original Passport is required).
- **BerryCore (sw7ft)** - Talkbutton dictation and the BerryCore userland run on
  the Passport.
- Hubs: CrackBerry (the "Passport running Lineage OS 18" thread), XDA,
  balika011.hu, and the BlackBerry community Discord.

## Repository layout

| Path | Contents |
|---|---|
| `notes/` | Passport session notes (driver forensics, dumps, EDL, FS_DIRTY) |
| `recon/bootloaders-imggen/` | the `imggen` prototype-bootloader kit (MSM8974) |
| `recon/dumps-passport/` | Passport eMMC dumps (`bb0_full.bin`, `os0_8M.bin`, …) |
| `tools/` | `passport_stage3`, `passport-root/` |
| `devmaps/` | Passport device map (schema v1.0) |
| `firmware/` | **not committed** — fetch instructions |

---

## Related repos

- **Hub:** [Blackberry-Research](https://github.com/stanw47/Blackberry-Research)
- **Classic** (same BB10 platform): [Blackberry-Classic-Research](https://github.com/stanw47/Blackberry-Classic-Research)
- **Priv** (adjacent MSM8992 lineage): [Blackberry-Priv-Research](https://github.com/stanw47/Blackberry-Priv-Research)

---

## Citations & Acknowledgements

| Source | URL | Relevance |
|---|---|---|
| balika011 — Passport conversion | https://balika011.hu/blackberry/guides/passport/conversion.php | canonical eMMC unlock |
| BBAndroids / imggen | https://github.com/BBAndroids/imggen | prototype bootloader |
| BBAndroids / passport_stage3 | https://github.com/BBAndroids/passport_stage3 | PBL debug-mode path |
| Oleksandr / bb10.root.sx | https://bb10.root.sx | RAM-loader / raw-MMC research |

---

## License

Research notes and original scripts are provided for educational purposes;
third-party code retains its own license.
