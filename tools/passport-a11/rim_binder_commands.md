# RIM (BB10 4.3) binder command values — extracted from the Passport libbinder

Source: `libbinder.so` from the Passport 4.3 runtime dump
(`recon/passport-runtime-4.3/`; local: `~/priv-research/working/passport-player43/
extracted/.../native/system/lib/libbinder.so`), literal pool at
`0x2308c` (read from `IPCThreadState::executeCommand`) plus the movw constants
in `joinThreadPool` and the check in RIM `libbionic.so`'s `ioctl_binder`.

## The rule: direction bits are SWAPPED vs Linux/AOSP

RIM's build defined `_IOC` with the classic **`_IOC_WRITE`=2, `_IOC_READ`=1**
(swapped relative to Linux: WRITE=1, READ=2). Struct **sizes are 32-bit-era**.
So the translation from an A11 (64-bit ABI) command word to RIM's word is:

1. replace the size field (bits 16–29) with the 32-bit-era struct size, then
2. swap the two direction bits:
   `rim = (c32 & 0x3FFFFFFF) | ((c32 & 0x40000000) << 1) | ((c32 & 0x80000000) >> 1)`

Verified: AOSP `BC_TRANSACTION` = `_IOW('c',0,64B)` = `0x40406300` → size 40 →
`0x40286300` → swap dir → **`0x80286300`** — exactly the value `ioctl_binder`
compares against (with `0x80286301` = `BC_REPLY`). `_IO` commands have dir 0 and
are unchanged.

## Verified values (from RIM's own code)

BR_ (driver → libbinder), read from the `executeCommand` pool (`0x2308c…`):

| RIM value | dir | size | nr | command |
|---|---|---|---|---|
| `0x40047200` | R(=1) | 4 | 0 | `BR_ERROR` |
| `0x40287202` | R | 0x28 | 2 | **`BR_TRANSACTION`** (32-bit txn data = 40 B) |
| `0x40087209` | R | 8 | 9 | `BR_RELEASE` (ptr_cookie = 8 B) |
| `0x400c720b` | R | 0xC | 11 | `BR_ATTEMPT_ACQUIRE` (RIM's desc is 12 B) |
| `0x4008720a` | R | 8 | 10 | `BR_DECREFS` |
| `0x40047210` | R | 4 | 16 | `BR_CLEAR_DEATH_NOTIFICATION_DONE` |
| `0x720d` … | – | 0 | 13 | `BR_SPAWN_LOOPER` (`_IO`) |
| `0x720e` … | – | 0 | 14 | `BR_FINISHED` (`_IO`) |

BC_ (libbinder → driver):

| RIM value | dir | size | nr | command |
|---|---|---|---|---|
| `0x80286300` | W(=2) | 0x28 | 0 | **`BC_TRANSACTION`** (32-bit txn data) |
| `0x80286301` | W | 0x28 | 1 | **`BC_REPLY`** |
| `0x80046302` | W | 4 | 2 | `BC_ACQUIRE_RESULT` |
| `0x80086308` | W | 8 | 8 | `BC_INCREFS_DONE` |
| `0x80086309` | W | 8 | 9 | `BC_ACQUIRE_DONE` |
| `0x80046310` | W | 4 | 16 | `BC_DEAD_BINDER_DONE` |
| `0x630b` | – | 0 | 11 | `BC_REGISTER_LOOPER` (`_IO`, unchanged) |
| `0x630c` | – | 0 | 12 | `BC_ENTER_LOOPER` (`_IO`, unchanged) |

Derive the rest with the rule (e.g. `BC_FREE_BUFFER` `_IOW('c',3,u32ptr)`:
AOSP `0x40046303` → size 4 → swap dir → `0x80046303`;
`BR_REPLY` = `0x40287203`; `BR_NOOP` = `0x720c`; `BR_OK` = `0x7201`;
`BR_DEAD_REPLY` = `0x7205`; `BR_TRANSACTION_COMPLETE` = `0x7206`;
`BR_FAILED_REPLY` = `0x7211`; `BR_DEAD_BINDER` = `0x4004720f`).

## Struct sizes (A11 64-bit ABI → RIM 32-bit)

| struct | A11 | RIM |
|---|---|---|
| `binder_write_read` | 48 | **24** |
| `binder_transaction_data` | 64 | **40** |
| `flat_binder_object` | 24 | **16** |
| `binder_ptr_cookie` | 16 | **8** |
| `binder_uintptr_t` / `binder_size_t` | 8 | **4** |
| `binder_version` | 4 | 4 (same) |

Transaction buffers also embed `flat_binder_object`s and (when
`TF_ONE_WAY`-style node refs / fd arrays are present) `binder_size_t` offset
arrays (u64 → u32), so the write/read buffer translation is a **walk**, not a
flat memcpy.

## Driver ioctls (via devctl; `ioctl_binder` ≡ `devctl`)

| dcmd | meaning |
|---|---|
| `0xC0046209` | `BINDER_VERSION` (4 B) |
| `0xC108620C` | ProcessState ctor setup (size field `0xfe000`) |
| `0xC03C620B` | `binder_qnx_fd()` transaction-memory query (60 B) |
| `0xC0186201` | `BINDER_WRITE_READ` (**24 B** — 32-bit layout) |

All request numbers for the driver use Linux `_IOWR` encoding with the **24-byte**
bwr — i.e. the driver resource manager expects the 32-bit ABI; the direction
swap above applies only to the **in-buffer BC_/BR_ command words**, not to the
devctl dcmds.
