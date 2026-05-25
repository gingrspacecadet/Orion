.Phony: all
all: asm emu disasm

.Phony: asm
asm: asm.c
	gcc -g -o asm asm.c

.Phony: emu
emu: emu.c
	gcc -g -o emu emu.c

.Phony: disasm
disasm: disasm.c
	gcc -g -o disasm disasm.c