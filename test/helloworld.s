__entry:
    lui r1, #0x0001
    add r1, r1, #0x0614 ; load PCU BAR addr
    lui r2, #0x003 ; desired MMIO location
    str r2, [r1]    ; store at BAR config

transmit:
    ; write the char back to ram
    mov r3, #0x48 strb [r3], r2
    mov r3, #0x65 strb [r3], r2
    mov r3, #0x6C strb [r3], r2
    mov r3, #0x6C strb [r3], r2
    mov r3, #0x6F strb [r3], r2
    mov r3, #0x2C strb [r3], r2
    mov r3, #0x20 strb [r3], r2
    mov r3, #0x77 strb [r3], r2
    mov r3, #0x6F strb [r3], r2
    mov r3, #0x72 strb [r3], r2
    mov r3, #0x6C strb [r3], r2
    mov r3, #0x64 strb [r3], r2
    mov r3, #0x21 strb [r3], r2
    mov r3, #0x0A strb [r3], r2

__exit:
    halt
;
;   Sets up the default handlers for interrupts
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