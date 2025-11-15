main:
    mov r0, #0xFFFE
    add r0, r0, r0
    mov r1, #0xB800
    str r0, r1, #0
    jmp $main