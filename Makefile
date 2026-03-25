.PHONY: all
all:
	@gcc -g asm.c -o asm
	@gcc -g emu.c -o emu

.PHONY: run
run: all
	@./asm test.asm
	@./emu a.out