start:
    mov r1, #0x000B
    shl r1, r1, #16
    or r1, r1, #0x8000
    mov r0, #0x4800
    shl r0, r0, #16
    str r0, r1, #0
    add r1, r1, #1
    mov r0, #0x6500
    shl r0, r0, #16
    str r0, r1, #0
    add r1, r1, #1
    mov r0, #0x6C00
    shl r0, r0, #16
    str r0, r1, #0
    add r1, r1, #1
    str r0, r1, #0
    add r1, r1, #1
    mov r0, #0x6F00
    shl r0, r0, #16
    str r0, r1, #0
    add r1, r1, #1
    mov r0, #0x2000
    shl r0, r0, #16
    str r0, r1, #0
    add r1, r1, #1
    mov r0, #0x5700
    shl r0, r0, #16
    str r0, r1, #0
    add r1, r1, #1
    mov r0, #0x6F00
    shl r0, r0, #16
    str r0, r1, #0
    add r1, r1, #1
    mov r0, #0x7200
    shl r0, r0, #16
    str r0, r1, #0
    add r1, r1, #1
    mov r0, #0x6C00
    shl r0, r0, #16
    str r0, r1, #0
    add r1, r1, #1
    mov r0, #0x6400
    shl r0, r0, #16
    str r0, r1, #0
    add r1, r1, #1