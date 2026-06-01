.section .text
__entry: 
    ; Disable IE flag
    mov flags, #0
    
    ; clear all registers
    xor r0,  r0,  r0
    xor r1,  r1,  r1
    xor r2,  r2,  r2
    xor r3,  r3,  r3
    xor r4,  r4,  r4
    xor r5,  r5,  r5
    xor r6,  r6,  r6
    xor r7,  r7,  r7
    xor r8,  r8,  r8
    xor r9,  r9,  r9
    xor r10, r10, r10
    xor r11, r11, r11
    xor r12, r12, r12
    xor r13, r13, r13
    xor r14, r14, r14
    
    ; setup temporary stack
    mov r15, r15, #0xFFFFFFFF
    
    ; mask all icu interrupts
    mov r1, #0x00010400
    str r15, [r1 + #0x8]

loop: jmp $loop