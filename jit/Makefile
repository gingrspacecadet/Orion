.PHONY: all
all:
	gcc -c jit.c -I../include
	gcc -c main.c  -I../include
	gcc -c cpu.c -I../invlude
	gcc jit.o main.o cpu.o
	./a.out	