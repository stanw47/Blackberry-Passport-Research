# Session 20 — boot0 forensic analysis: RPMB-gated FS_DIRTY_ALL flag

- **Date:** 2026-09-19
- **Device:** Passport (SQW100)
- **Access level:** offline (boot0 dump analysis)
- **Status:** finding

> Preserved from the pre-split `Blackberry-Research` README (remote commit
> `8da2c5c`, "Document boot0 forensic analysis and recovery steps").

## TL;DR

After a raw CMD6 experiment against the eMMC, the Passport sits in the
**11011** (Flash Erase Failure) bootrom state; all autoloaders fail at ~13%
("Signature Trailer"). A 4 MiB `boot0` dump (`bb0_full.bin`) is structurally
intact (real GPT + signed SBL1). The blocker is the **`FS_DIRTY_ALL` flag,
which SBL1 reads from RPMB** — not from plain NVRAM/user area. RPMB is
hardware-HMAC-protected with a device-unique key and monotonic counter, so
restoring `boot0`/`boot1`/`user` from backups (even chip-off) does **not**
clear it.

## Context

Passport in 11011 bootrom state following a raw CMD6 experiment while
researching write-protect behavior. Autoloaders (stock, pre-rooted, custom) all
fail at ~13% ("Signature Trailer"). A 24-hour battery disconnect had no effect —
consistent with the failure state living in persistent flash, not RAM.

## Verified structure of `bb0_full.bin`

- Real GPT header at file offset `0x200` (`EFI PART`, backup LBA `0x7ff`).
- Single GPT partition entry **`BootROM`**, LBA `0x22`–`0x1ff`
  (file offset `0x4400`–`0x40000`).
- Qualcomm MBN header at `0x4400`: load addr `0xF800C000`, image size
  `0x2F4D4`, code size `0x2DBD4`, signature size `0x100`, cert chain `0x1800`.

This is a structurally intact, correctly-formed **signed SBL1 image** — not
corrupted boot0 data.

## Key finding: `FS_DIRTY_ALL` is RPMB-backed

SBL1 payload strings:

```
FS_DIRTY_ALL flag is set, trapping OS start
Error retrieving FS_DIRTY flag from RPMB
FS_DIRTY_ALL flag is set, must perform bootrom level Nuke
Error retrieving FS_DIRTY flag from RPMB
```

The flag is stored in **RPMB (Replay Protected Memory Block)**, not a plain
NVRAM/user field as earlier sessions hypothesized.

### Why this matters

RPMB is a hardware-enforced eMMC region: writes need a valid HMAC against a
device-unique key derived once in TrustZone and never readable out, plus a
correctly incrementing monotonic counter (replay rejected). **Consequence:** a
full restoration of `boot0`/`boot1`/`user` from known-good backups — even at
chip-off level — does **not** touch RPMB, so it will not clear `FS_DIRTY_ALL`.

### Unresolved

Automated literal-pool tracing from the `FS_DIRTY_ALL` strings to the
`MMCFlashErase*` call did not resolve cleanly (PC-relative addressing). Whether
the "bootrom level Nuke" targets the protected `MCT_BOOTROM` region (matching
the 11011 symptom) or another range is open. A proper disassembly pass or a live
UART capture would resolve it.

## Recovery implications

1. **UART console** (if test points found) is the highest-value next step —
   shows the actual FS_DIRTY/Nuke sequence live.
2. **Chip-off restore of the *same* eMMC** is unlikely to resolve it (RPMB
   persists independent of boot0/user data).
3. **Full eMMC chip replacement** (fresh, unprovisioned chip) is the more
   promising path — a blank chip has no RPMB key provisioned, so the boot chain
   re-provisions on first boot, plausibly resetting `FS_DIRTY_ALL`.
4. Forging a valid RPMB write externally is not practically achievable without
   the device-derived key or a TrustZone exploit.

## Artifacts

| File | What it is |
|---|---|
| `bb0_full.bin` | 4 MiB boot0 dump (not committed — see `firmware/FETCH.md`) |

## Next

- Locate UART test points; capture the live FS_DIRTY/Nuke sequence.
