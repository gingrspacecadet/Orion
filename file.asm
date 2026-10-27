.section .text
.global _start
_start:
    lui rsp, #1
    div r0, r0, #0 ; triggers fault
    halt

handler:
    add r0, r0, #69 ; proof it happened
    halt

.section .ihvt
.word $handler
.word $handler
.word $handler
.word $handler
.word $handler
.word $handler
.word $handler
.word $handler
.word $handler
.word $handler
