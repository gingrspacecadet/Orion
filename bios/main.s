; BIOS pseudo-assembly using $label references for addresses
; - 0x00001234 = 0x1234 (RAM word index)
; - 0x00002000 = 0x2000 (RAM word index)
; - INT #0 : exit BIOS -> emulator sets mode = USER and PC = R0
; - INT #1 : IRET -> emulator restores saved user context
; - Uses LDR/STR for RAM accesses; uses $label to embed addresses where needed

BOOT:
    ; -- zero the tick counter --
    MOV   R0, #0
    MOV   R1, #0x00002000        ; R1 <- literal RAM index where tick counter lives
    STR   R0, R1, #0             ; mem[0x00002000] = 0

    ; -- initialize IVT[0] = address of TIMER_HANDLER (ROM index) --
    MOV   R2, $TIMER_HANDLER     ; R2 <- ROM word index of TIMER_HANDLER
    MOV   R3, #0x00001234        ; R3 <- RAM index of IVT base (0x1234)
    STR   R2, R3, #0             ; mem[0x00001234] = TIMER_HANDLER

    ; -- boot handoff: set R0 to user entry and exit BIOS --
    MOV   R0, #0x00000000        ; R0 <- user program entry
    INT   #0x0000                ; emulator: leave BIOS and jump to R0
    HLT

; -------------------------
; Timer interrupt handler in ROM
; -------------------------
TIMER_HANDLER:
    PUSH  #0x000F                ; save R0-R3

    MOV   R4, #0x00002000        ; R4 = tick counter RAM index
    LDR   R5, R4, #0             ; R5 = mem[0x00002000]
    ADD   R5, R5, #1             ; R5 = R5 + 1
    STR   R5, R4, #0             ; mem[0x00002000] = R5

    POP   #0x000F                ; restore R0-R3
    IRET                         ; IRET (emulator restores user context)
    HLT

; -------------------------
; Data / constants (assembler resolves $labels)
; -------------------------
; Place these in ROM image or define in your linker script:
;   $TIMER_HANDLER  -> resolved to the ROM word index of TIMER_HANDLER
;   $0x00001234       -> 0x00001234
;   $0x00002000      -> 0x00002000
;   $0x00000000     -> 0x00000000 (or your program entry)
