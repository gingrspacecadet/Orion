.org #0x0
__entry:
mov r1, #0xF
halt

.org #0x00001000
ihvt:
.word #0x0
.word #0x0
.word #0x0
.word $__stack_underflow

__stack_underflow:
    mov r12, #0xFFFFFFFF
    iret
