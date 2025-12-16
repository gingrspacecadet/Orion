int main() {
    // test
    /* test 2! */
    asm("    MOV   R15, #0

    MOV   R0, #0
    MOV   R1, #0x00002000
    STR   R0, R1, #0

    MOV   R2, $TIMER_HANDLER
    MOV   R3, #0x00001234
    STR   R2, R3, #0

    MOV   R2, $IRQ1_HANDLER
    ADD   R3, R3, #1
    STR   R2, R3, #0

    CALL $idle

TIMER_HANDLER:
    PUSH  #0x000F

    MOV   R4, #0x00002000
    LDR   R5, R4, #0
    ADD   R5, R5, #1
    STR   R5, R4, #0

    POP   #0x000F
    IRET
    HLT

IRQ1_HANDLER:
    PUSH  #0x000F
    MOV   R1, #0x000B
    SHL   R1, R1, #16
    OR    R1, R1, #0x8000

.loop:
    MOV   R2, #0x00FF
    SHL   R2, R2, #16
    LDR   R0, R2, #0
    CMP   R0, #0
    JE    $.done

    STR   R15, R1, #0
    ADD   R15, R15, #1
    SHL   R0, R0, #24
    ADD   R3, R1, #1
    STR   R0, R3, #0
    JMP   $.loop

.done:
    POP   #0x000F
    IRET
    HLT

idle:
    JMP   $idle
");
    return 0;
}