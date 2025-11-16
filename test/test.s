start:
    mov r1, #0x000B     ;
    shl r1, r1, #16     ;   VGA address
    or r1, r1, #0x8000  ;
    jmp $main

str:
.str Hello

printchar:
    shl r0, r0, #16 ; align
    str r0, r1, #0  ; store
    add r1, r1, #1  ; increment
    ret

main:
    mov r2, #0
    ldr r0, r2, $str
    call $printchar
    add r2, r2, #1
    ldr r0, r2, $str
    call $printchar
    add r2, r2, #1
    ldr r0, r2, $str
    call $printchar
    add r2, r2, #1
    ldr r0, r2, $str
    call $printchar
    add r2, r2, #1
    ldr r0, r2, $str
    call $printchar
