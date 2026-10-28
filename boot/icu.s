.section .text
.global icu_init
icu_init:
    push {r4, r5}           ; prologue

    mov r4, #0x00010400     ; ICU address
    mov r5, #0xFFFFFFFF     ; IMR mask value (masks all interrupts)
    str r5, [r4 + #0x08]    ; set the mask

    pop {r4, r5}            ; epilogue
    ret