# BlackBerry Passport (SQW100) — Research

> Root, boot-chain, driver-forensics, and no-desolder unlock research on the
> BlackBerry **Passport** (BB10 / QNX, MSM8974AA).
>
> Part of the **[Blackberry-Research](https://github.com/stanw47/Blackberry-Research)**
> collection. Cross-device mechanisms live in the hub; this repo is Passport-specific.

---

## Disclaimer

> **Research aid, not a flashing guide.** Editing eMMC boot partitions or
> toggling write-protect can **permanently brick** the device. The Passport is
> currently non-bootable — recovery is the priority. For educational/defensive
> research on devices the author owns. At your own risk.

---

## Status

| Field | Value |
|---|---|
| Device / model | BlackBerry Passport (SQW100) |
| SoC | Qualcomm MSM8974AA (Snapdragon 801) |
| OS / build | BB10 / QNX (`QNX BLACKBERRY-603C`, WINDERMEREEMEA) |
| Bootloader | locked; boot-partition WP permanent |
| Root | rooted (uid-0) — but device currently **red-blink / non-bootable** |
| Access levels | L0 usb, L1 fastboot, L4 qnx (when bootable) |
| Status | **recovery pending**; no-desolder Android path mapped via `imggen` |

**Current state:** The Passport was rooted, but after a reboot it entered a
**red-blink loop** (the documented stub/payload recovery issue). Autoloader
recovery now fails at the "Signature Trailer" step. Recovery is the open task;
the no-desolder Android conversion path is mapped.

---

## TL;DR

- **Passport = MSM8974AA** — the exact target of the public **`imggen`**
  prototype-bootloader toolchain, so the Android/unlock path is far more
  promising than on the Classic (MSM8960).
- **No-desolder path:** RAM-loader lane + `imggen` boot0/user images +
  `ext_csd[179]=0x08` → fastboot → recovery → LineageOS.
- **`passport_stage3`** exposes the RPM→PBL debug-mode
  (`BOOT_PARTITION_SELECT=0x5D1`) route.
- **Driver forensics complete:** the boot0 write-protect gate is decoded; `ext`
  is per-open-node, not global.
- **Recovery is the blocker:** the device red-blinks; the install-seal
  (`F9 40` SIGNATURE_TRAILER) contract explains the stub failure.

---

## Key findings

*Numbered, stable — append only.*

1. **Passport boot0 holds SBL1 (1 MiB); boot1 is blank**; `os0` holds the QNX IFS.
   → [`notes/session19-driver-gate-decode-and-passport-dumps.md`](notes/session19-driver-gate-decode-and-passport-dumps.md)
2. **`imggen` + `passport_stage3`** decoded: prototype bootloader + RPM/PBL
   debug-mode unlock path. → hub `cross-device/` + [`tools/`](tools/)
3. **MMC driver gate (`0xF182`) / WP handler (`0x108D0`)** fully decoded;
   `ext` is per-open-node.
4. **`/dev/mem` is a decoy** (uniform `0xdeadbeef`).
5. **Install-seal contract (560 bytes)** explains the red-blink vs `cap.exe`.
6. **NVRAM unlock flags** (bits 42/43) persist but only gate the official
   updater, not the running block layer.

---

## How to connect

Same BB10 Dev-Mode SSH ritual as the Classic (fresh key, `blackberry-connect`
tunnel, paramiko). See hub
[`docs/ssh-connection-linux.md`](https://github.com/stanw47/Blackberry-Research/blob/main/docs/ssh-connection-linux.md).
When non-bootable, the device only enumerates in **BootROM/RAM-loader** mode
(listener-first; see `recon/`).

---

## Repository layout

| Path | Contents |
|---|---|
| `notes/` | Passport session notes (driver forensics, dumps, EDL signature) |
| `docs/` | write-ups (to be filled) |
| `recon/bootloaders-imggen/` | the public `imggen` prototype-bootloader kit (MSM8974) |
| `recon/dumps-passport/` | Passport eMMC dumps (SBL1, boot1, os0 IFS) |
| `tools/passport_stage3/`, `tools/passport-root/` | Passport-specific tooling |
| `devmaps/` | Passport device map (schema v1.0) |
| `firmware/` | **not committed** — fetch instructions |

---

## Related repos

- **Hub:** [Blackberry-Research](https://github.com/stanw47/Blackberry-Research)
- **Classic:** [Blackberry-Classic-Research](https://github.com/stanw47/Blackberry-Classic-Research)
- **Priv:** [Blackberry-Priv-Research](https://github.com/stanw47/Blackberry-Priv-Research)

---

## References

| Source | URL | Relevance |
|---|---|---|
| balika011 Passport conversion | https://balika011.hu/blackberry/guides/passport/conversion.php | canonical eMMC unlock |
| BBAndroids/imggen | https://github.com/BBAndroids/imggen | prototype bootloader |
| BBAndroids/passport_stage3 | https://github.com/BBAndroids/passport_stage3 | PBL debug-mode path |
| bb10.root.sx | https://bb10.root.sx | RAM-loader / raw-MMC research |

---

## License

Research notes and original scripts are provided for educational purposes;
third-party code retains its own license.
