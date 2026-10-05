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
| Model | BlackBerry Passport **SQW100** |
| Codename | `passport` / `windermere` |
| SoC | Qualcomm **MSM8974AA** (Snapdragon 801) |
| OS / software | **BB10 / QNX**, `BLACKBERRY-603C`, WindermereEMEA |
| Current build | 10.3.3.3216 (rooted autoloader) |
| Previous builds | stock 10.3.3 |
| Carrier / unlock | carrier-unlocked; bootloader locked; boot-partition WP permanent |
| SIM | single |

---

## Current Status

The Passport was **rooted (uid-0)**, but after a raw `CMD6` experiment it entered
a **red-blink loop** (`11011` "Flash Erase Failure") and is currently
**non-bootable**. Autoloader recovery fails at ~13% ("Signature Trailer"). The
blocker is now understood: **`FS_DIRTY_ALL` is stored in RPMB**, so restoring
`boot0`/`boot1`/`user` (even chip-off) will **not** clear it. The device's real
strength is that **MSM8974AA is the exact `imggen` target**, making a no-desolder
Android conversion feasible once it boots.

---

## Completed

- **Root** (previously) via the getroot/pathtrust ritual.
- **Driver forensics:** the MMC WP gate (`0xF182`), WP handler (`0x108D0`), and
  the per-open-node `ext` model fully decoded; CID is live-read; no ext_csd cache.
- **eMMC dumps:** `boot0`, `boot1`, `os0` recovered.
- **`FS_DIRTY_ALL` forensics:** shown to be RPMB-backed.
- **`imggen` / `passport_stage3`** decoded (prototype bootloader + PBL debug path).

## Achieved

- ✅ **Driver write-protect model fully mapped** (why the software lane is closed).
- ✅ **`FS_DIRTY_ALL` root-caused to RPMB** — a decisive negative for chip-off restore.
- ✅ **No-desolder Android path identified** (`imggen` boot0/user, `ext_csd[179]=0x08`).
- ✅ **Per-node `ext` insight** — identical WP probes diverge by open context.

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

1. Locate **UART test points** and capture the live FS_DIRTY/Nuke sequence.
2. Compare **chip-off restore vs full eMMC chip replacement** (a blank chip
   re-provisions RPMB).
3. Once bootable: the no-desolder `imggen`/RAM-loader Android conversion.

---

## Community Activity

- **balika011's conversion guide** is the canonical end-to-end unlock (desolder →
  `imggen` boot0/user → `ext_csd[179]=0x08` → fastboot → recovery → LineageOS).
- **BBAndroids** published `imggen` (GPL) and `passport_stage3`; the community
  shares rooted autoloaders. The **no-desolder** path is the active frontier.

---

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
