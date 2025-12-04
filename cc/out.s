_start:
    MOV r13, #0xFFFF    ; init SP (word address)
    SHL r13, r13, #16
    OR  r13, r13, #0xFFFF
    MOV r12, r13    ; init FP
    MOV r0, #0
    MOV r1, #0
    CALL $main
    MOV r1, r0
    INT #0
    HLT

main:
    PUSH #0x9000    ; save FP(r12) and LR(r15)
    MOV r12, r13
    MOV r0, #10
    STR r0, r12, #2    ; store local slot 1
    LDR r0, r12, #2    ; load local slot 1
    POP #0x9000    ; restore FP and LR
    RET
