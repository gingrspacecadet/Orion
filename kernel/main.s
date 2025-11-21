start:
    mov r0, #0x00FE
    shl r0, r0, #16

    mov r1, #0
    str r1, r0, #3
    str r1, r0, #4
    str r1, r0, #5

    mov r1, #6
    str r1, r0, #6

    mov r1, #98
    str r1, r0, #0
    mov r1, #119
    str r1, r0, #0
    mov r1, #97
    str r1, r0, #0
    mov r1, #104
    str r1, r0, #0

    mov r1, #3
    str r1, r0, #2

.loop:
    jmp $.loop
