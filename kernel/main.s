start:
    mov r1, #0x000B
    shl r1, r1, #16
    or r1, r1, #0x8000
    mov r0, #0xFFFF
    shl r0, r0, #16
    or r0, r0, #0xFF00

loop:
    add r1, r1, #1
    str r0, r1, #0
    jmp $loop
