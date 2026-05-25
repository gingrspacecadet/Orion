;
;   standard program entrypoint abi thing.
;

__entry:    call $main
__exit:     halt

main:
    mov r1, #0xFFFFFFFF
    push r1
    pop r1
    ret