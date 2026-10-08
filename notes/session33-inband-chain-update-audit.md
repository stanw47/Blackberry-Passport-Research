# Session 33 — In-band boot-chain update audit: can the stock chain be adapted
# (OS/user replaced) without writing boot0?

Date: 2026-10-08. Follows session28–32. Question: can we "build on" the
write-protected chain — keep boot0, replace OS/IFS/user/radio via the official
flashing process — and still get Android? Answer: **content yes, chain no.**

## 1. What the update process can write

- QCFM record types are user-area only (`nvram, ufs, mbr, sig, ifs, rcfs,
  radio.*, calwork, calbackup, dmi, os, sig2…`) — no record type targets
  `boot0`/`boot1` (session15 + `qcfm.pas` type table).
- The **IFS is verified at boot**: a 1-byte IFS change → red-blink refusal
  (session9b). So the kernel/boot image cannot be replaced with custom content.
- RCFS (OS filesystem), UFS (user), MBR and radio **can** be replaced — the
  pre-rooted autoloaders are the proof.
- ⇒ "Adapt the OS/user image to the stock chain" gives full **userspace/OS
  control** and the Android *user-area layout*, but the executed kernel remains
  QNX. It does not run Android.

## 2. Is there any in-band chain writer? (all negative)

| candidate | result |
|---|---|
| QCFM record type for boot region | none (public types + MCT kinds don't map) |
| BB10 OS services (`backup_bootchain`, `bkup_bc`, BCB) in root RCFS/IFS | **absent** (string scan, 393 MB RCFS + 10 MB IFS) |
| Retail chain (boot0): secondary-boot management | **state only**: `MCT_BOOTROM_SEC_NAND`, `Primary bootrom detected secondary failed to boot, %d secondary boot attempts remaining`, `Failed to update Secondary boot NVRecords`, WP applied to "secondary boot loader" — no write/update routine strings |
| Android prototype chain | **has** the writer (`Updating backup bootchain`, `phyboot0/1`, `update-bootchain`, BCB at `/nvram/perm/bootcontrolblock`, staging `bcota`) — Android-only mechanism, absent on BB10 |
| RAM-loader (BOOT0's restoration protocol) | MCT/bootrom *read* + "Flash error due to Write protection"; raw write-op semantics undecoded (loader is position-independent; strings contain no absolute pointers — needs proper RE) |

## 3. The nuance that keeps one software door ajar

`sessions 15/18/20`: posing NVOSSTORE `0x2019` byte5 `0x0C` ("boot partition
write allowed") tells the **official updater** that SBL1/boot0 must be written
during a flash. The old board then failed the boot-region erase → `11011`
deadlock, which was read as "permanent WP". Our live retail read shows
`ext_csd[173] = 0x04 = B_PWR_WP_EN` (**power-on, not fused**) — so the wall may
be narrower than thought:

- The updater **has** a boot0/boot1 write path (NV-gated);
- it is blocked by the boot chain's power-on WP, re-applied each boot, which the
  card **rejected clearing** via CMD6 on the Classic (session9c) — retail eMMC
  behaviour unverified;
- therefore the remaining software question is whether the *retail* eMMC accepts
  a CMD6 clear of `B_PWR_WP_EN` (then NV-pose + autoloader could write the chain).

Sources weighed: primary RE (bb-usbdl: BOOT0 = proprietary SBL1/2/3 replacement
+ RAMLoader, no QDL), community consensus (postmarketOS wiki: eMMC replacement
required to unlock), our live `ext_csd`/string/updater analysis. Foundation
Devices' "passport-firmware" is a **different product** (Bitcoin wallet) — not
relevant.

## 4. Conclusion / routes for Android on the retail

1. **Hardware (proven)**: programmer/eMMC swap + imggen (Balika). Fully
   de-risked by our work: exact boot/user images, byte-identical autoloader
   tooling, LineageOS package, and recovery ritual all in hand.
2. **Software (unresolved)**: retail-eMMC CMD6-clear acceptance test + NV pose +
   official updater's boot0-write path — or the RAM-loader's raw boot-region ops
   (undecoded, by-design wipe risk). Both need deeper loader/MMC work.
