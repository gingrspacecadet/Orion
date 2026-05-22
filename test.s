.org 0x100
start:
    LUI R1, #0x1234
    ADD R2, R1, #4
    PUSH R2
    CALL abs handler
    HALT

handler:
    POP R2
    RET
