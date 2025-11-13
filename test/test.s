main:
    mov r2, #0x200
    mov r3, r2
    jmp $loop
    mov r4, #0x1000
    halt
loop:
    jmp $main