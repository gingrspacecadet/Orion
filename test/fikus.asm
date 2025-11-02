        ldi R0, 5
        str R0, MEM[200]      ; number of elements (5)

        ldi R0, 9
        str R0, MEM[100]
        ldi R0, 4
        str R0, MEM[101]
        ldi R0, 7
        str R0, MEM[102]
        ldi R0, 1
        str R0, MEM[103]
        ldi R0, 3
        str R0, MEM[104]

sort_start:
        ldi R0, 0
        str R0, MEM[200]       ; outer = 0

outer_loop:
        ldr R0, MEM[200]       ; R0 = outer index
        ldi R1, 5
        sub R1, R1, R0         ; R1 = 5 - outer
        cmp R1, 1              ; if R1 <= 1, done
        jz done
        jnz continue_outer

continue_outer:
        ldi R0, 0
        str R0, MEM[201]       ; inner = 0

inner_loop:
        ldr R1, MEM[201]       ; R1 = inner
        ldi R2, 4              ; R2 = 4 (max inner index = n-2)
        cmp R1, R2
        jz next_outer
        jnz compare_elements

compare_elements:
        ; Load MEM[100 + inner] into R2
        ldr R1, MEM[201]
        ldi R0, 100
        add R0, R0, R1
        ldr R2, MEM[R0]        ; R2 = arr[inner]

        ; Load MEM[101 + inner] into R3
        ldr R3, MEM[201]
        add R3, R3, 101
        ldr R3, MEM[R3]        ; R3 = arr[inner + 1]

        cmp R2, R3
        jz no_swap
        jnz maybe_swap

maybe_swap:
        ; if R2 > R3, swap them
        sub R4, R2, R3
        cmp R4, 0
        jz no_swap
        jnz do_swap

do_swap:
        ; Store R3 into arr[inner]
        ldr R1, MEM[201]
        ldi R0, 100
        add R0, R0, R1
        str R3, MEM[R0]

        ; Store R2 into arr[inner+1]
        ldr R1, MEM[201]
        add R1, R1, 101
        str R2, MEM[R1]

no_swap:
        ldr R0, MEM[201]
        ldi R1, 1
        add R0, R0, R1
        str R0, MEM[201]
        jmp inner_loop

next_outer:
        ldr R0, MEM[200]
        ldi R1, 1
        add R0, R0, R1
        str R0, MEM[200]
        jmp outer_loop

done:
        hlt