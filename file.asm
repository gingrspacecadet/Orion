add r1, r2, #-1
add r1, r2, r3
nop
ldr r2, r3, r4
symbol:
lui r4, #42

jmp $symbol
call $extern
.section .data