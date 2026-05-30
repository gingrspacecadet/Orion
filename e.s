__entry: jmp $__entry

__default_int_handler:
    add r1, r1, #1
    halt
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