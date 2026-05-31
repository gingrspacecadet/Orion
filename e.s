__entry:
    mov r1, #0x00010A00

    mov r2, #0x00020000
    str r2, [r1]

    mov r3, #1
    str r3, [r1 + #0xC]

    mov r4, #0xFFFF
    str r4, [r2 + #0x12]
loop: jmp $loop

__default_int_handler:
    iret
.section .ihvt
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler
.word $__default_int_handler