CC = gcc
CFLAGS = -g -O2 -Wall $(shell pkg-config --cflags glib-2.0)
LDFLAGS = $(shell pkg-config --libs glib-2.0`)
FLEX = flex
BISON = bison

all: ccomp

lexer.c: lexer.l
	$(FLEX) -o lexer.c lexer.l

parser.c parser.h: parser.y
	$(BISON) -d -o parser.c parser.y

ccomp: lexer.c parser.c ast.o codegen.o main.o
	$(CC) $(CFLAGS) -o ccomp lexer.c parser.c ast.o codegen.o main.o

ast.o: ast.c ast.h
	$(CC) $(CFLAGS) -c ast.c

codegen.o: codegen.c ast.h
	$(CC) $(CFLAGS) -c codegen.c

main.o: main.c parser.h ast.h
	$(CC) $(CFLAGS) -c main.c

clean:
	rm -f ccomp lexer.c parser.c parser.h *.o a.out *.s
