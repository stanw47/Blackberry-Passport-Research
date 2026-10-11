# Session 53 — conversion image set for a NEW board (imggen-native)

Date: 2026-10-11. Target: a **new** retail Passport board connected live
(BLACKBERRY-E538, `wolverine`/`emea`, hwid `0x87002c0a`, pcb_rev 13, pop_rev 3,
real_rev 13, **secure** unit). Distinct physical unit from session35's board
(same model/rev; unit-specific nvram/cal differ).

## Connection (Linux)

Followed the repo doc `stanw47/Blackberry-Research/docs/ssh-connection-linux.md`:

1. Device: Settings → Security and Privacy → **Development Mode ON**
   (does not persist across reboot). Check ports: `4455 OPEN`, `22 closed`,
   `5555 OPEN` = dev mode confirmed, btool not yet run.
2. Fresh **4096-bit** RSA key (device enforces 4096 min; keys are wiped on
   disconnect/reboot):
   `ssh-keygen -t rsa -b 4096 -f /tmp/bb_key -N "" -q`
3. `blackberry-connect 169.254.0.1 -password <devpw> -sshPublicKey /tmp/bb_key.pub`
   detached. It authenticates on 4455, pushes the key, and triggers `btool`
   (which restarts sshd → port 22 opens).
4. paramiko SSH as `devuser` with **QNX quirk**: RSA-SHA1 only. Requires both
   `disabled_algorithms={'pubkeys':['rsa-sha2-512','rsa-sha2-256']}` AND a
   Transport monkeypatch `kw.setdefault('server_sig_algs', False)`.
   Helper: `/tmp/opencode/bbssh2.py` (this session).
5. Root via `echo '<cmd>' | /base/bin/__root` (works when btool whitelisted it).

## Dumps (as root)

```
dd if=/dev/emmc/boot0 of=... bs=512     count=8192    # 4,194,304 B
dd if=/dev/emmc/user0 of=... bs=1048576 count=128     # 134,217,728 B (128 MiB)
```

- boot0 sha256 `75108c66…` — byte-identical to the session19 retail dump (the
  signed BB10 boot chain is unit-generic; identity + MCT live here).
- 128 MiB user prefix is exactly what imggen needs: MCT `cal_backup` ends at
  eMMC `0x8200000`; imggen reads `nvram`/`cal_work`/`cal_backup` at
  `(mct_offset - 0x200000)` within the user image (window `0x4000000..0x8000000`).

## Board identity (parsed from boot0 MCT/build info)

`tools/conversion/parse_bb0.py <boot0>` prints build info + MCT table.
Product `wolverine`, variant `emea`, **secure=True** (→ imggen uses the secure
10-part boot GPT), pcb_rev 13, pop_rev 3 (real_rev 13 ≥ 5, passes imggen's check).

## imggen (native)

```
~/Downloads/imggen src_bb0.img src_user0_prefix.img new_boot0.img new_user.img
```
- `new_boot0.img` 3,896,320 B — secure 10-part boot GPT
- `new_user.img`  151,515,136 B — Android 40-partition user GPT + unit data
- Sizes equal the session35 set. Boot image is board-generic (same hwid/rev),
  so only the **user** image carries the new unit's calibration.

## Aboot nuance (important)

Native imggen output embeds the **prebuilt's aboot (1,952,122 B)**. The imggen
GitHub kit `files/aboot.mbn` is **342,264 B** (md5 `eaf75dafc2a245cb976dc3a9c9c29023`)
and is byte-identical to the `aboot` inside Balika's Android UFS payload
(session30) — i.e. the aboot that actually lands on converted devices. Both are
AUTHBOOT-free. Swapping abootr/aboot with the kit aboot makes `new_boot0.img`
byte-identical to session35's `boot_secure.img` (md5 `461d3168…`), so the
session35 boot image already equals this board's boot image.

Both variants delivered (see MANIFEST): native and `*_kitaboot.img`.

## Where the files go (Balika route)

- `new_boot0*.img` → eMMC **boot partition 1**; mark boot0 as boot partition
  (`ext_csd[179] = 0x08`).
- `new_user*.img` → eMMC **user area at `0x200000`** (prefix; Android
  system/userdata flashed later via fastboot recovery).

## Artifacts

Durable: `~/priv-research/working/passport-conversion-newboard/`
(`new_boot0.img`, `new_user.img`, `new_boot0_kitaboot.img`,
`new_user_kitaboot.img`, `src_bb0.img`, `src_user0_prefix.img`, `MANIFEST.txt`).

## Follow-ups

- Decide native vs kit-aboot for the actual programming (kit aboot = what
  converted devices carry; native = raw imggen).
- If a loader/OS-side write path is ever built, these are the images to carry.
