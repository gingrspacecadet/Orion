.PHONY: all
all:
	gcc -c jit.c -I../include
	gcc -c main.c  -I../include
	gcc -c cpu.c -I../include
	gcc -c memory.c -I../include
	gcc jit.o main.o cpu.o memory.o
	./a.out	