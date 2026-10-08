# Passport — Summary

- **Device:** BlackBerry Passport (SQW100)
- **SoC:** MSM8974AA
- **OS/build:** BB10 / QNX (`QNX BLACKBERRY-603C`, WINDERMEREEMEA)
- **Repo:** https://github.com/stanw47/Blackberry-Passport-Research
- **Visibility:** private
- **Status:** current board (`BLACKBERRY-E538`) bootable + rooted; old board
  parked (red-blink); Android prototype (`oslo`, AAA787) preserved
- **Headline:** the **Android 11 native chain runs on the Passport**
  (`tb_a11` → libutils+libbinder loaded, `ProcessState::self`, `operator new`)

## Headlines
- Full BB10 4.3 Android runtime dumped from the Passport (container + var +
  appdata, 1,179-file manifest) — `recon/passport-runtime-4.3/`.
- **A11-on-QNX chain runs on the Passport.** Root cause of the long pre-init
  hang: NEEDED link order (`libc.so.3` must come before `libc.so`). See
  `notes/session39`.
- **BB10 binder driver interface discovered** in RIM's `libbionic.so`
  (`ioctl_binder`) — request numbers + ProcessState ctor sequence; note the
  32-bit (RIM 4.3) vs 64-bit (A11) wire-format gap. See `notes/session40`.
- MSM8974AA = exact `imggen` target → viable no-desolder Android/unlock path.
- `passport_stage3` RPM→PBL debug mode (`BOOT_PARTITION_SELECT=0x5D1`).
- MMC boot-WP gate decoded; `ext` is per-open-node.
- Install-seal (`F9 40`) contract explains the red-blink vs `cap.exe`.

## Blockers
- Binder: A11 libbinder ↔ BB10 `/dev/binder` needs RIM's `ioctl_binder`
  translation + 32↔64-bit wire translation, or a different transport.
- Old board non-bootable (red-blink); needs live CMD6 / RAM-loader write / ISP.
- boot-partition WP power-on (temporary) — re-applied each boot.

## Last updated
2026-10-08
