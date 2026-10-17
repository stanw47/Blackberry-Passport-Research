# Session 70 — Mining balika011's "Blackberry Android hideout" Discord (Passport/oslo)

Date 2026-10-18. Source: three DiscordChatExporter dumps of the **Blackberry
Android hideout → Passport ‑ SQW100 (oslo / wolverine / keian)** channels:
`general` (1.86 MB), `conversion-help` (315 KB), `tutorials` (1.1 KB). Raw exports
kept locally (not committed — they contain many third‑party messages/PII).

## balika011's canonical resources (from `tutorials`)
- MEGA folder: `https://mega.nz/folder/Pq5AnAzL#KYPYiihMh5MynUZNLIE86g` (17.75 GB).
- Firmwares: `http://balika011.hu/blackberry/firmwares/` →
  **`passport/lineage/`** = `recovery.img` (15 MB, 24‑Dec‑2024), `recovery-adb.img`,
  **`lineage-18.1-20250101-UNOFFICIAL-wseries.zip`** (526 MB) and a `‑microg` build
  (563 MB); plus `passport/official complete/` and `official incomplete/` (the
  Guizmox prototype dumps).
- Teardown: `https://balika011.hu/blackberry/guides/passport/teardown.php`
- **BB10→Android conversion guide:** `https://balika011.hu/blackberry/guides/passport/conversion.php`

## Conversion guide (balika011.hu) — the real procedure
> "Currently only **V5 and newer** passports supported. SE and AT&T guaranteed."
1. Download platform-tools + **imggen** + latest **LineageOS recovery and rom**.
2. (Retail/BB10) remove logic board → remove eMMC shield → **de‑solder the eMMC** →
   programmer **backup of boot0 + user**, then `imggen.exe boot0.img user.img
   new_boot0.img new_user.img` → write `new_boot0`/`new_user` to the **new** chip,
   `ext_csd 179 = 0x08` (boot0 bootable) → solder back.
3. Reassemble → boots to **fastboot** (WHT/BLU/RED/GRN error = missing boot img).
4. `fastboot flash recovery recovery.img`
5. Vol‑Up + Power → **yellow LED** → recovery → Factory reset → Format data →
   Apply update → Apply from ADB → `adb sideload lineage-18.1-*.zip` → Reboot.

So the **desolder is only for retail/BB10**; an **unlocked prototype** skips step 2
and goes straight to flashing recovery + sideloading LOS.

## Key facts from `general` (balika011 unless noted)
- **Unlock rate:** "Not all prototypes, but most of them. So far we only found **one
  prototype out of 8 that had signature checking, and one with write protected, but
  unlocked BB10 bootloader**." ⇒ ~6/8 prototypes are unlocked/unsigned-boot.
- **The signed BB10 bootrom loads UNSIGNED ramloaders:** "this is a secure signed
  *bootrom* (bb10 bootloader) for passport that **can load unsigned ramloaders** …
  if we could somehow get this running on the devices we also won" (attached
  `phyboot0.bin`). ⇒ the RAMLoader signature check is absent/weak — the no‑desolder
  write primitive.
- **"For the passport we don't even need to swap"** (once the loader path works).
- **Two ways to boot a converted chain:** "you need **secure signed bbss and prod
  signed sbl1 + matching blobs** to boot" … "or **use the secureboot exploit i'll
  publish**" (12‑Dec‑2024).
- **boot0 locked ⇒ use boot1:** "boot0 is also locked on os10 pp, but we can **make
  boot1 default and flash android bootloader there**."
- **balika uses BlackBerry‑SIGNED LKs**, not custom: "we don't use a custom lk. we
  use ones **signed by blackberry** we found on prototypes from the landfill."
- **LOS install on an unlocked prototype:** `fastboot flash recovery recovery.img`
  then recVERY → `adb sideload`; "you can **skip the fastboot oem unlock part**".
- **balika's oslo is rev `164`** ("my oslo is 164"); ours reports `rev=131076`
  (0x20004) — different field/encoding, to reconcile.
- AAC014 (5.1) is **backed up** ("we have a copy, you can flash the phone with it").
- Retail attempts (bbfan) with AAB928/AAC014 dumps + ext_csd tweaks → **no boot**;
  "it's really not just changing a couple of files".
- `BBTool_backup.exe` is a Discord drop (attached in `general`); a `BBTool_switch.exe`
  also exists.

## Consequences for our oslo
- The unlocks revolve around the **boot area** (`boot0`/`boot1`) and the
  **unsigned‑ramloader** path — not the OS/user area. Matches session64–67.
- **Cheap decisive test of our unit's lock state:** flash **balika's `recovery.img`**
  via `fastboot flash recovery` and attempt to boot it (Vol‑Up→recovery, yellow LED).
  If the LOS recovery boots ⇒ our unit is **unlocked** ⇒ we can `adb sideload` the
  `wseries` LOS zip directly (balika's path). If the LK rejects it ⇒ locked ⇒ we need
  the boot‑area/loader route.
- Data to grab next: `recovery.img`, `lineage-18.1-…-wseries.zip` from
  `balika011.hu/blackberry/firmwares/passport/lineage/`; the MEGA folder; and the
  `official complete/incomplete` Guizmox dumps for reference.
