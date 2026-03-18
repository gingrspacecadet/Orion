add:
    mov r0, r15
    add r0, r0, #2
    ldr r0, r0, #0
    mov r1, r0
    mov r0, r15
    add r0, r0, #3
    ldr r0, r0, #0
    mov r2, r0
    mov r0, r1
    add r0, r0, r2
    ret

main:
    mov r0, #3
    call $add
    ret

