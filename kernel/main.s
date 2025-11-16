start:
    jmp $main

printchar:
    shl r0, r0, #16
    str r0, r1, #0
    add r1, r1, #1
    ret

main:
    mov r1, #0x000B
    shl r1, r1, #16
    or r1, r1, #0x8000
    mov r0, #0x4800
    call $printchar
    mov r0, #0x6500
    call $printchar
    mov r0, #0x6C00
    call $printchar
    mov r0, #0x6C00
    call $printchar
    mov r0, #0x6F00
    call $printchar
    mov r0, #0x2000
    call $printchar
    mov r0, #0x5700
    call $printchar
    mov r0, #0x6F00
    call $printchar
    mov r0, #0x7200
    call $printchar
    mov r0, #0x6C00
    call $printchar
    mov r0, #0x6400
    call $printchar