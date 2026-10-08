# Session 38 — A11 native chain runs on the Passport (link-order root cause)

**Milestone:** the Android 11 native runtime chain now executes on the retail
Passport (E538):

    tb_a11 -> [SHIM] init
              tb_a11: sigaction rc=0x0
              UBS N=0x10c86770    # libutils+libbinder loaded, ProcessState::self
                                  # resolved, libc++ operator new returns a valid
                                  # heap pointer
              RC=0

**Root cause of the long-standing pre-init hang/crash:** the probes' NEEDED
order. The shim (`libc.so`) must NOT be loaded before the real `libc.so.3`:
`[libc.so, libc.so.3]` spins inside load-time resolution (silently, READY
state); `[libc.so.3, libc.so]` works. The A11 probes linked `libc++.so` first,
which pulled `libc.so` into the link map ahead of `libc.so.3`. Fix: link
`-l:libc.so.3 -l:libc++.so -l:libc.so` (see the Classic repo session76 note
for the full bisection and the debug-tooling details, incl. why `pdebug`
attach is blocked by BB10's /proc policy).

The WS1 shim also gained link-time alias binding (`libqnxbind.so`,
`qnxb_ptrs[]`) during this work, removing runtime dlopen/dlsym from the
resolver entirely.

Next: call `ProcessState::self()` for real (open `/dev/binder` on the
Passport) via `probe_binder_step` with the fixed link order.
