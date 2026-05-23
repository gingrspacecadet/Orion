.Phony: all
all: asm emu

.Phony: asm
asm:
	gcc -o asm asm.c

.Phony: emu
emu:
	gcc -o emu emu.c