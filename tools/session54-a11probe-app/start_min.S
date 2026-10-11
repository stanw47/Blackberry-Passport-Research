.syntax unified
.arch armv7-a
.text
.global _start
.type _start, %function
_start:
    ldr r0, [sp]
    add r1, sp, #4
    add r2, r1, r0, lsl #2
    add r2, r2, #4
    mov r4, r0
    mov r5, r1
    mov r6, r2
    bl _init_libc
    mov r0, r4
    mov r1, r5
    mov r2, r6
    bl main
    bl exit
    b .
.size _start, .-_start
