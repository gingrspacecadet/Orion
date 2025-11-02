ldi R1, #0        ; F0 = 0
ldi R2, #1        ; F1 = 1

ldi R3, #0        ; clear temp for next = R1 + R2
add R3, R1        ; R3 += R1
add R3, R2        ; R3 = R1 + R2

mov R1, R2        ; shift: R1 <- R2
mov R2, R3        ; shift: R2 <- R3 (new current)
push R2

jmp #0x6          ; jump back to the instruction whose first token is at index 6

hlt               ; done
