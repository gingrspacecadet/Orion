mov R1, #0
mov R2, #1

loop:
    mov R3, #0
    add R3, R1, R2

    mov R1, R2
    mov R2, R3

    jmp $loop

hlt
