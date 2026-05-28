.org #0x00001000
__entry:
    mov r0, #5
    mov r1, #1
    mov r2, #1
loop:
    add r3, r1, r2
    xor r2, r2, r3
    xor r3, r2, r3
    xor r2, r2, r3
    mov r1, r3
    sub r0, r0, #1
    jne $loop
__exit:
    halt
