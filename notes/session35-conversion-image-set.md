# Session 35 — Conversion image set for the replacement board (Balika-faithful)

Date: 2026-10-08. Target: the **replacement board** in the retail Passport
(`BLACKBERRY-E538`, `wolverine`/`emea`, `hwid 0x87002c0a`, pcb_rev 13, pop_rev 3,
HWI chksum `0x7eaef1ed`).

## Inputs

- `retail_boot0.img` + `retail_user_prefix.img` (this board, dumped via root).
- GitHub imggen kit `priv-research/imggen/files/` (chain components + GPT
  templates).
- Prebuilt imggen (`~/Downloads/imggen`) outputs `new_boot1.img` /
  `new_user_gpt.bin` (board data: HWI, nvram, cal).

## Key correction vs the raw imggen output

The **prebuilt imggen embeds a different engineering aboot** (1,952,122 B,
md5 `6642603c…`) than the GitHub kit / balika package (342,264 B,
md5 `eaf75daf…`). Both are AUTHBOOT-free; the kit one is Balika-faithful, so it
was patched into every generated image.

## Artifacts (durable copy: `~/priv-research/working/passport-conversion/`)

| image | size | md5 |
|---|---|---|
| `boot_secure.img` | 3,896,320 | `461d31686411cef8a010b6d553cd709c` |
| `boot_insecure.img` | 3,429,376 | `d0d8068c7b4e9ee68f9eb02827694270` |
| `user.img` | 151,515,136 | `e0377c4b79eb463156444235187b3e12` |

- `boot_secure.img` — 10-part boot GPT (`hwi, stage1, stage2, stage3, bbss,
  sbl1r, tzr, rpmr, sdir, abootr`); `stage3` = the `passport_stage3` exploit
  (session31). The correct image for a **secure** unit's `boot0`.
- `boot_insecure.img` — 7-part boot GPT (`hwi, bbss, sbl1r, tzr, rpmr, sdir,
  abootr`), the prototype backup-chain shape (no exploit bootstrap).
- `user.img` — user-area prefix (Android 40-partition GPT + board data + kit
  chain images incl. the engineering `aboot`) → eMMC `0x200000`.

All chain components verified **byte-identical to the GitHub kit**; both GPTs
parse and their CRCs are valid.

## Where they go (for the hardware/loader write)

- boot image → eMMC **boot partition 1** (`boot0`); `boot_insecure.img` may also
  be used for `boot1` A/B experiments.
- `user.img` → eMMC offset `0x200000` (user area).
- These are ready for the programmer (Balika route) or a future loader-side
  boot-region write; they do not by themselves enable an OS-side write.

## Tool

`tools/conversion/make_conversion_set.py` — deterministic rebuild from the
prebuilt imggen outputs + kit dir (verified: reproduces all three images
byte-identically). Fixed along the way: Python bytearray slice-assignment past
EOF inserts instead of zero-extending (use an explicit extend; earlier ad-hoc
builds had done this correctly).
