# Session 66 — LK (aboot) verified-boot / unlock path + the oslo → LineageOS route

Date 2026-10-18. Follows session64/65. Device: oslo Android prototype (LMY47D
AAA787). Goal (user): boot **LineageOS 18.1** on it, **reproducibly**, to help
other owners of the "dubai" batch (QWERTY + Arabic keyboard; siblings 5.0.2, this
unit 5.1) and eventually to port an Android upgrade to the **retail Passport**.

Context from the community (Discord): the ~Jul-2015 AAC014/AAB982 build family
ships **all the tools to convert BB10 devices to Android**; because **boot0 is
unwritable**, BlackBerry started Android from the **original BB10 bootloader at
the LK (LittleKernel) stage** (LittleKernel source reference in Discord
`#…/1293863361343258726`). AAC014 is the newest build (System=AAC014 /
Modem=AAB057 / Kernel=AABxxx); ~10 builds survive (AAA097…AAC014).

--------------------------------------------------------------------------------
## 1. LK verified-boot decision (device `partition-dumps/aboot.img`, 4 MiB)

The production LK gates the HLOS on a triple, logged as
**`use_signed_kernel=%d, is_unlocked=%d, is_tampered=%d.`**
(file `0x0ab490`, VA `0x1016f490` with base `0x100c4000`). Flow strings:

| file off | string |
|----------|--------|
| `0x0ab3ac` / `0x0ab3b8` | `recoverysig` / `bootsig` (signature partitions) |
| `0x0ab408` | `ANDROID!` (boot magic) |
| `0x0ab490` | `use_signed_kernel=%d, is_unlocked=%d, is_tampered=%d.` |
| `0x0ab568` | `Authenticating boot image (%d): start` |
| `0x0ab590` | `Authenticating boot image with signature %d: done return value = %d` |
| `0x0ab5d8` | `ERROR: Cannot validate Device Tree Table` |

Code (ARM, `app/aboot/aboot.c`): the boot-image loader/validator around file
`0x017640-0x017a80` (VA `0x100db640-0x100dba80`) prints
`Authenticating boot image (%d): start` (VA `0x1016f568`) and calls the
**signature verifier at `0x100d5ea0`**; a result `!= 1` → failure. So
authentication is **signature-based** and is what rejects a custom recovery/boot.

### Fastboot / authboot permission gates
- `authboot command permission denied` (file `0x0ac0b0`),
  `user does not have permission to run command %s` (`0x0ac074`),
  `user does not have permission to erase %s partition.` (`0x0ab078`),
  `authboot erase permission denied` (`0x0ab0b0`), `Authboot perm init failed`
  (`0x0abef4`). → LK fastboot enforces a per-command/per-partition allow-list
  (session63: only `boot`/`recovery` flash succeeds).
- Fastboot mode strings: `VOL_UP+VOL_DN pressed. Continue booting.`,
  `VOL_UP pressed. Recovery image.`, `VOL_DN pressed. Fastboot mode.`,
  `ERROR: Could not do normal boot. Reverting to fastboot mode.`,
  `fastboot: processing commands`, `fastboot: %s`, `getvar:`, `download:`.
- **`oem` command set (file `0x0b0438+`): `switchboot, mmcinfo, ddrinfo,
  enable/disable-charger-screen, info, bootlog, securewipe, grswipe,
  enable-usb-reset, led:, bootchart, getvarp:, dmesg`.** There is **no
  `oem unlock` / `flashing unlock`** → `is_unlocked` is *not* settable from
  fastboot on this build.
- `all_debug_tokens` (`0x0ab0d4`) + `failed removing all debug tokens` → an
  `oem`-side debug-token erase exists, but debug tokens are device-bound and
  dead (no servers).

### Conclusion on §1
The LK **does** verify the HLOS with the production key and **does not** expose a
fastboot unlock. So the oslo→LOS route must supply `is_unlocked=true` (unknown
setter) **or** replace the LK **or** supply images signed with a key the LK
trusts (impossible). This is the same wall as session62/63.

--------------------------------------------------------------------------------
## 2. Corroboration: boot0 is OS-unwritable (session15)

- `pwrite(/dev/emmc/boot0, …)` → **EROFS** even after setting NV `0x2019` bits
  42/43 (`00 08 00 00 00 0C …`, persisted). The bits gate only the **official
  updater** path (autoloader flashing SBL1), not OS `/dev` writes.
- session15 explicitly named a spare **`AAW068 Passport-SE / STV100-1 / oslo`**
  as the intended first target for the (unproven) BootROM boot0 flash.
- ⇒ the boot area can only be written via the **BootROM/official-updater** lane
  (session65), never from the running OS.

--------------------------------------------------------------------------------
## 3. The crux for oslo → LOS: which area does the LK load from?

The device holds **both** a user-area `aboot` (4 MiB, `block_device`, *vold-writable*)
and a boot-area `abootr`. The Discord note ("Android started from the original
BB10 bootloader at LK state", boot0 unwritable) implies the **LK is the writable
lever**, i.e. the **user-area `aboot`**. session64's BBSS disassembly instead read
the SBL GPT from the *detected boot partition*. These conflict; resolving it
decides the whole route:

- **If the LK is the user-area `aboot`** (vold-writable): we can write a modified
  LK from the OS that skips HLOS verification → then flash LOS recovery/boot.
  Purely OS-side — the clean, reproducible route.
- **If the LK is the boot-area `abootr`**: only the BootROM/official-updater lane
  can change it (needs an oslo boot chain + oslo RAM-loader).

### Decisive read-only tests (next)
1. Read `/dev/emmc/boot0` (if readable despite EROFS) and compare its `sbl1r`/
   `abootr` regions against the user-area `sbl1`/`aboot` dumps already captured.
2. From the LK, log/inspect where `boot_linux_from_mmc` reads the image and where
   the LK itself was linked/loaded (boot args, `oem bootlog`, `blog` partition).
3. Confirm whether the user-area `aboot` is genuinely OS-writable via the
   vold-root channel (session63 write-matrix) — a bounded, reversible test with a
   backup of the original 4 MiB `aboot`.

--------------------------------------------------------------------------------
## 4. Route options (least → most invasive)

1. **LK-stage edit of user-area `aboot`** (if §3 says user-area): patch/swap the
   LK to skip HLOS verification, then `fastboot flash recovery` (LOS) + boot.
   Reproducible from the OS; also the mechanism BlackBerry itself used per Discord.
2. **`is_unlocked`** — find the setter (none in fastboot; maybe factory/NV) —
   otherwise dead.
3. **Boot-area chain via BootROM lane** — build an **oslo** insecure boot chain
   from the `F:\imggen` kit (`boot_gpt_insecure.bin` + oslo `hwi` + stage1/2/3 +
   bbss + sbl1r + tzr/rpmr/sdir + engineering `abootr`) and obtain an **oslo**
   RAM-loader; flash via the official-updater path the `0x2019` bits enable.
4. **Retail-Passport port (later):** same LK-stage principle (boot Android from
   the immutable BB10 LK), packaged from the AAC014-family tooling.

--------------------------------------------------------------------------------
## 5. Pipeline for LineageOS 18.1 (from the community)
Balika used AAA628 (Feb-2015) blobs; the newer **AAC014** (Jul-2015) blobs are
preferred. Devices that "can't be upgraded to LOS" are suspected **v2 vs V4**
hardware revisions. This unit is **v1/AAA787** with a 5.1 kernel
`3.4.0-grsec-g0ea3e38` — confirm its revision class before assuming LOS
compatibility.
