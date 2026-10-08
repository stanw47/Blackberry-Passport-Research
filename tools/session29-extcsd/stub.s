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
FN devctl
FN setgid
FN setuid
FN exit
FN _init_libc
FN _preinit_array
FN _init_array
.section .data
.global errno
.type errno, %object
errno:
    .word 0
