# Passport — Summary

- **Device:** BlackBerry Passport (SQW100)
- **SoC:** MSM8974AA
- **OS/build:** BB10 / QNX (`QNX BLACKBERRY-603C`, WINDERMEREEMEA)
- **Repo:** https://github.com/stanw47/Blackberry-Passport-Research
- **Visibility:** private
- **Status:** rooted but red-blink / non-bootable; recovery pending
- **Headline:** `imggen` (MSM8974) prototype-bootloader no-desolder unlock path mapped

## Headlines
- MSM8974AA = exact `imggen` target → viable no-desolder Android/unlock path.
- `passport_stage3` RPM→PBL debug mode (`BOOT_PARTITION_SELECT=0x5D1`).
- MMC boot-WP gate decoded; `ext` is per-open-node.
- Install-seal (`F9 40`) contract explains the red-blink vs `cap.exe`.

## Blockers
- Device non-bootable (red-blink); autoloader recovery fails at Signature Trailer.
- boot-partition WP permanent; needs live CMD6 / RAM-loader write / ISP.

## Last updated
2026-10-05
