start:
    mov r1, #0x000B
    shl r1, r1, #16
    or r1, r1, #0x837B
    jmp $main

printchar:
    str r0, r1, #0
    add r1, r1, #1
    ret

main:
    mov r2, #0        ; byte offset into str

loop:
    ldr r0, r2, $str  ; load word containing byte at offset r2
    shl r0, r0, #24   ; bring target byte into top 8 bits
    cmp r0, #0        ; test for terminator (0)
    je $done
    call $printchar
    ldr r0, r2, $str
    shl r0, r0, #16
    cmp r0, #0
    je $done
    call $printchar
    ldr r0, r2, $str
    shl r0, r0, #8
    cmp r0, #0
    je $done
    call $printchar
    ldr r0, r2, $str
    add r2, r2, #1
    cmp r0, #0
    je $done
    call $printchar
    jmp $loop

done:
    jmp $done

str:
    .str Uh oh! Something went wrong :( Guess you gotta restart :3
