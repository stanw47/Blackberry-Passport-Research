# Session 57 — RIMBOOT (retail boot0) verification model + boot-chain flaw hunt

Date 2026-10-13. Device: retail Passport (Windermere EMEA, MSM8974AA). Goal (user):
find a **flaw in the boot chain** so the device boots "something else" (Android)
**without replacing the chain** — i.e. no boot0 write. Secondary: a flaw in the
**userspace handoff** ("hand over ownership of the boot process/storage/hardware").

Artifact: `recon/dumps-passport/bb0_full.bin` (boot0). `boot1` is empty.
Tooling: radare2 6.2.4 (Windows) + capstone; base `0xF800C000`.

---

## 1. RIMBOOT identity (boot0)

- boot0 = a Qualcomm **MBN** image, self-described **"RIM-BootLoader" / "RIM-BootNUKE"**.
- Header @ file 0x4400 (bb0): magic `0xD1DC4B84`, load base **`0xF800C000`**,
  `image_size 0x2F4D4`, `code_size 0x2DBD4`, signature @ `+0x39BD4` (0x100),
  cert chain @ `+0x39CD4` (0x1800), oem_root_cert_sel=1.
- Code starts at file 0x50 (VA `0xF800C050`): an **ARM** vector table, then
  **Thumb-2** body; some leaf routines are **ARM** (called via `blx`).
- GPT: one entry "BootROM" (LBA 34–511) at the SBL1 header.

## 2. Boot flow

`PBL → RIMBOOT → load+verify QNX OS image (user area / os0) → jump`.
Multiple OS types: OS / **installer OS** / **RFOS** (`overrideOSTypes`).

## 3. The OS is ECC-signature verified (no naive swap)

`fcn.f80209aa` (OS verify) does, per OS slot (1..2):
- `fcn.f8020170` "Loading OS" → `fcn.f8020482` (RPMB/PLI) → copy/decompress.
- **"Verifying OS ECC"** `fcn.f802107c` and **"Verifying OS CRC"** `fcn.f80212a4`.
- Signature verifier **`fcn.f8028970` (ARM)**:
  - signature block at `image_end-0x230`; **magic `0xB5A60BFD` at +0xB8**;
    a size/offset field at +0x20 (must not be 0/`0xffffffff`); sig at +0x28.
  - hashes the image (`fcn.f80286fc`, streaming, 2 MiB chunks) → gets root cert
    by id **`0x85`** (`fcn.f80292f4`) → **ECC verify** (`fcn.f8029544`).
- **Preload images (PLIs)** use the *same* verifier (`fcn.f800c638`,
  `fcn.f8020316`): "OS preload images are NOT signed" / "Default preload images
  are NOT signed" are **fatal** (return 0).

**"Bind/Verify failed"** (`fcn.f8023222` → `fcn.f800db86`) is **processor
binding** (MSMID / KEK / `IsProcessorBound`), **not** the OS signature.

## 4. MCT (Media Control Table) is eMMC geometry, not an OS locator

`fcn.f8022464`: magic **`0x92BE564A`**, entry types (`0x09` CRC, `0xff` end),
CRC-checked (`fcn.f8020804`). Stored in NVRAM (id `0x2028`) / "old location" /
default; "Writing MCT to %d:%d". It describes **NAND/eMMC geometry** — it does
**not** point at the OS image.

## 5. Implications for "boot something else" (no boot0 write)

1. **OS image is ECC-signed (RIM PKI)** → cannot be replaced outright.
2. The **PKI is shared** with the prototype/balika images (session 22/29), but
   those are **Android MBN** blobs, not a QNX OS image carrying the
   `0xB5A60BFD` signature block — so the RIMBOOT will not boot them as the OS.
3. Candidate flaws, ranked:
   - **A. Signed-range vs executed-range mismatch.** The verifier hashes
     `[base, base+len)`; the loader (`fcn.f8020dfa`/`fcn.f8020b4a`) copies/
     decompresses. If `len` (image field / structure) can be made smaller than
     what is executed, we get a **valid-signature-but-unsigned-code** image.
     → the highest-value target; next: fully decode `fcn.f8028970` size math and
     the OS image header it trusts.
   - **B. Preload images (PLIs).** Loaded early via RPMB/PLI provisioning
     (`fcn.f8020482`, `fcn.f800c638`). If a PLI location/size comes from a
     writable record, inject there.
   - **C. Userspace handoff.** RIMBOOT jumps to the QNX image; we control the
     writable RCFS/UFS. But the kernel is QNX → booting **Linux** needs a ring0
     handoff (kexec), i.e. a QNX kernel bug — separate workstream.

## 6. Where this leaves the goal

- The **guaranteed** no-boot0 route to Android is still **Android-on-QNX**
  (route #2: keep the chain, run our A11 runtime on the QNX kernel). Harness is
  in `Blackberry-Classic-Research/runtime/a11-build` + `ws1/`.
- The **boot-chain flaw** route needs A or B to pan out. Next concrete step:
  fully decode `fcn.f8028970` (size math) + the OS image header, and test whether
  a signed-but-short image executes beyond its signed range.

## 7. Artifacts

- `bb0_full.bin` (boot0), extracted `rimboot.bin` (1 MiB @ 0x4400).
- r2 project + this decode; function addresses above are VA (base 0xF800C000).
