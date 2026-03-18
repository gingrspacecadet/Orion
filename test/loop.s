main:
    mov r2, #0x200
    mov r3, r2
    call $loop
    mov r4, #0x1000
    hlt
loop:
    ret