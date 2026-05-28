.org #0x00001000
__entry:
    lui r1, #0x0001
    add r1, r1, #0x0614 ; load PCU BAR addr
    lui r2, #0x0003 ; desired MMIO location
    str r2, [r1]    ; store at BAR config

echo_loop:
    ldr r3, [r2 + #4]   ; read status register
    
    ; check RX_READY for keypress
    and r4, r3, #1
    cmp r4, #0
    jeq $echo_loop  

    ldrb r5, r2, #0 

    ; just for shiggles, capitalise lowercase letters
    cmp r5, #'a'
    jlt $transmit
    
    cmp r5, #'z'
    jge $transmit
    
    sub r5, r5, #0x20

transmit:
    ; write the char back to ram
    strb r5, r2, #0

    cmp r5, #'Q'
    jne $echo_loop

__exit:
    halt
;
;   Sets up the default handlers for interrups
;   so the system doesn't crash
;
__default_panic_handler:
    halt
    jmp $__default_panic_handler

.org #0x00010000
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler
.word $__default_panic_handler