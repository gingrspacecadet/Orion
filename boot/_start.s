.section .text
.global _start
_start:
    ;-- Initialise the cpu! --;

    ; ensure supervisor mode
    flagsw #2    ; FLAG_PRV

    ; set up stack
    mov r15, #0xFFFFFFFC

    ;-- initialise basic devices --;
    ; initialise the interrupt controller
    call $icu_init
    
    ; launch the kernel!
    ; for now, the emulator loads it at address 0x00020000
    jmp #0x00020000

    ; just in case
    halt
