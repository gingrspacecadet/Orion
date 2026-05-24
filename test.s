__entry:
    mov r0, #5
    mov r1, #1
    mov r2, #1
loop:
    add r1, r1, r2
    xor r1, r1, r2
    xor r2, r1, r2
    xor r1, r1, r2
    sub r0, r0, #1
    cmp r0, #0
    jne $loop
__exit:
    halt