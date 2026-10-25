.global _start
_start:
    add r0, r0, #4
    mul r0, r0, #2
    sub r0, r0, #1
    jmp #1
    div r0, r0, #7