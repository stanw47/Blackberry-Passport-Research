.syntax unified
.arm
.section .text
.macro FN name
.global \name
.type \name, %function
\name:
    b \name
.endm
FN printf
FN open
FN close
FN write
FN devctl
FN getuid
FN geteuid
FN getgid
FN getegid
FN resmgr_attach
FN __get_errno_ptr
FN exit
FN _init_libc
FN _preinit_array
FN _init_array
.section .data
.global errno
.type errno, %object
errno:
    .word 0
