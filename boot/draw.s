;
; draw_char(r0: ascii, r1: x, r2: y, r3: colour)
;
draw_char:
    ; calculate pointer to font data for this character
    ; font_ptr = font_base + (ascii * 16)
    shl r4, r0, #4           ; r4 = ascii * 16
    mov r5, $font            ; load relocatable address of font array
    add r4, r5, r4           ; r4 = absolute pointer to character data

    ; initialise row loop counter
    mov r6, #0               ; r6 = row_idx (0 to 15)

row_loop:
    jge r6, #16, $char_done  ; if row_idx >= 16, we finished the character

    ; fetch font byte for this row and advance pointer
    ldrb r7, [r4 + #0]       ; read byte bitmask for current row
    add r4, r4, #1           ; advance font pointer to next row byte

    ; calculate exact framebuffer address for start of this row
    ; addr = fb_base + (((y + row_idx) * 320) + x) * 2
    add r9, r2, r6           ; r9 = current_y = y + row_idx
    shl r10, r9, #8          ; r10 = current_y * 256
    shl r11, r9, #6          ; r11 = current_y * 64
    add r10, r10, r11        ; r10 = current_y * 320
    add r10, r10, r1         ; r10 = (current_y * 320) + x
    shl r10, r10, #1         ; r10 = byte_offset = pixel_index * 2 (RGB565)
    
    mov r11, #0x00020000     ; framebuffer base address
    add r10, r11, r10        ; r10 = target destination pointer in VRAM

    ; initialise pixel loop counter (8 bits per row)
    mov r8, #0               ; r8 = bit_idx (0 to 7)

pixel_loop:
    jge r8, #8, $row_done    ; completed all 8 horizontal pixels

    ; test the most significant bit
    and r12, r7, #0x80 
    jeq r12, #0, $skip_pixel  ; if bit is 0, leave background intact

    ; paint the pixel
    strb r3, [r10 + #0]      ; store lower 8 bits of RGB565 colour
    shr r13, r3, #8          ; shift upper bits down
    strb r13, [r10 + #1]     ; store upper 8 bits of RGB565 colour

skip_pixel:
    shl r7, r7, #1           ; shift row mask left by 1 (next bit becomes MSB)
    add r10, r10, #2         ; advance framebuffer address to next pixel
    add r8, r8, #1           ; bit_idx++
    jmp $pixel_loop

row_done:
    add r6, r6, #1           ; row_idx++
    jmp $row_loop

char_done:
    ret

;
; draw_string(r0: string_ptr, r1: x, r2: y, r3: color)
;
draw_string:
    push r4                  ; preserve registers we plan to modify
    push r5
    mov r4, r0               ; r4 = Current string index pointer
    mov r5, r1               ; r5 = Tracking X screen position
    mov r6, #0               ; r6 = NULL

str_loop:
    ldrb r0, [r4]            ; fetch character ascii byte
    jeq r0, r6, $str_done    ; hit null terminator, break out

    push r4
    push r5
    
    or r1, r5, r5
    call $draw_char
    
    pop r5
    pop r4

    add r4, r4, #1           ; move pointer to next string character
    add r5, r5, #8           ; displace horizontal layout cursor by 8 pixels
    jmp $str_loop

str_done:
    pop r5
    pop r4
    ret