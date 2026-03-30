label:
; comment
add r1, r0, r3  ; add r1 and r0 and put the result in r3
sub r1, #0x14   ; subtract 14 from r1 and put the result in r1

jmp $foo    ; relative jump to label "foo", the '$' operator gets the relative offset to the label
jnea #0x1   ; absolute jump to 0x0000_0001 but *only if* the Z flag is clear

foo: jmp #0xFFFFFFFF