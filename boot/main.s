; .section .text
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
    xor r15, r15, r15

    ; setup temporary stack
    lui r15, #0xFFFF
    or r15, r15, #0xFFFF
    
    ; mask all icu interrupts
    lui r1, #0x1
    or r1, r1, #0x400
    str r15, [r1 + #0x8]

    ; init the gpu
    mov r0, #0x00010A00
    mov r1, #0x00020000
    str r1, [r0]
    mov r1, #1
    str r1, [r0 + #0xC]

    ; print "HELLO WORLD" at coord 20, 40 in bright white (0xFFFF)
    mov r0, $msg_hello
    mov r1, #20
    mov r2, #40
    mov r3, #0xFFFF
    call $draw_string

; .word #0xFFFFFFFF ; TODO: SHOULD FAULT!!

spin:
    halt
    jmp $spin

.section .data
msg_hello:
    .byte #72, #101, #108, #108, #111, #44, #32, #119, #111, #114, #108, #100, #0 ; "HELLO WORLD\0"