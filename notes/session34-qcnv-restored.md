# Session 34 — qcnv (Oleksandr's Qualcomm NV tool) restored to a working state

Date: 2026-10-08. Device: retail Passport (rooted). Source: `bb10root/qcnv`
(clone at `/tmp/opencode/qcnv`, upstream commit `6598cf6` by Oleksandr,
2026-10-07).

## 1. Problem and fix

The tool crashed at startup (SIGSEGV at entry) — the old arm-none-eabi build
used the broken crt (same class of bug fixed in session32). Rebuilt with the
proven recipe:

- `start_min.S` — `_init_libc(argc,argv,envp)` → `main` → `exit` (no
  `_preinit_array`/`_init_array` calls),
- auto-generated symbol stub (`stub2`, SONAME `libc.so.3`, 53 symbols incl.
  `dlopen`/`dlsym`/`pthread_*`/`stat`/`mkdir`/`stderr`),
- `-pie -z now`, stub includes from `qnxshim/` + `qnxinc/qnx_compat.h` + `-I .`
  for `banner.h`.

**Result: the tool runs** — banner + usage print, all `dlopen()`s succeed, QMI
client initializes (no lib or symbol errors).

## 2. Environment findings

- `libqmi_rim_vs.so` lives in **`/radio/lib`** (not `/base/lib`); the tool's
  internal `LD_LIBRARY_PATH` construction includes it.
- `stp-handler-dc-qct.so`, `libqcci_ext.so` are under `/base` (present).
- Run as root (`__root`), pathtrust-whitelisted.

## 3. NV read state (open)

`qcnv_tool read 8217` → "Could not read". With the DEBUG response dump enabled:

- 4-byte request → response `{item_id=0, result_code=0, data_len=1}` (i.e. the
  modem answered, but not with the requested item id).
- 140-byte padded request (per the author's comment "modem expects a 140-byte
  packet") → the call fails outright.

So the **QMI transport works**; the remaining item is the exact
`READ_NV_ITEM` request layout (Oleksandr's active RE frontier). The tool is
usable for `backup`/`restore`/EFS work once the packet is settled.

## 4. Local patches (candidates to upstream)

1. `pthread_mutex_init(&g_nv_mutex, 0)` in `init_external_symbols()` instead of
   the static `PTHREAD_MUTEX_INITIALIZER` (our shim lacks the initializer).
2. DEBUG response hex dump enabled in `nv_read_item`.
3. Request-pad experiment (136 zero bytes after `item_id`) — kept for
   comparison, does not fix the read.

## 5. Artifacts

- Working binary: `/tmp/opencode/qcnv_tool_dbg` (deployed as
  `/accounts/devuser/qcnv_tool`).
- Build (per `qc_nv_lib.c` / `qcnv_tool.c`):
  `arm-none-eabi-gcc -marm -O0 -DDEBUG -std=gnu99 -fPIC -ffreestanding -I . -I
  /tmp/opencode/qnxshim -I .../qnxinc -include qnx_compat.h -include
  qcnv_extra.h -c ...` then `arm-none-eabi-ld -pie -z now
  --dynamic-linker=/usr/lib/ldqnx.so.2 -e _start -L /tmp/opencode/stub2
  -o qcnv_tool start_min.o qcnv_tool.o qc_nv_lib.o -l:libc.so.3`.
