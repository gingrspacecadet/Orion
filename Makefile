.Phony: all
all: asm emu

.Phony: asm
asm: asm.c
	gcc -o asm asm.c

.Phony: emu
emu: emu.c
	gcc -o emu emu.c