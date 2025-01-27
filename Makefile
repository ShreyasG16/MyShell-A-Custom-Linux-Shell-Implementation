.PHONY: all clean

all:
	gcc -o MyShell shell.c
	./MyShell

clean:
	rm -f MyShell


