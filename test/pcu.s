__entry:
    add r1, r1, #0x1000
    add r3, r3, #16

scan_loop:
    cmp r2, r3
    jeq $halt_sys

    shl r4, r2, #4
    add r4, r1, r4

    ldr r5, [r4]
    cmp r5, r0
    jeq $next_slot

    shr r6, r5, #8
    and r6, r6, #0xFF

    add r7, r0, #0x02
    cmp r6, r7
    jne $next_slot
found_device:
    lui r8, #0
    add r8, r8, #0x4000
    add r8, r8, #0x4000

    str r8, [r4 + #4]

    add r9, r0, #'H'
    str r9, [r8]
    add r9, r0, #'e'
    str r9, [r8]
    add r9, r0, #'l'
    str r9, [r8]
    add r9, r0, #'l'
    str r9, [r8]
    add r9, r0, #'o'
    str r9, [r8]
    add r9, r0, #','
    str r9, [r8]
    add r9, r0, #' '
    str r9, [r8]
    add r9, r0, #'w'
    str r9, [r8]
    add r9, r0, #'o'
    str r9, [r8]
    add r9, r0, #'r'
    str r9, [r8]
    add r9, r0, #'l'
    str r9, [r8]
    add r9, r0, #'d'
    str r9, [r8]
    add r9, r0, #'!'
    str r9, [r8]
    add r9, r0, #0xA ; no escape literals yet
    str r9, [r8]
halt_sys:
    halt
next_slot:
    add r2, r2, #1
    jmp $scan_loop