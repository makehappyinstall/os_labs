CC=gcc
C_STD=c11
C_FLAGS=-Wall -Werror -Wpedantic -std=$(C_STD) -D_GNU_SOURCE
LIBS=-pthread -lpthread

all: main

main: Main.o IO.o
	$(CC) $(C_FLAGS) $(LIBS) Main.o IO.o -o main

Main.o:
	$(CC) $(C_FLAGS) -c Main.c

IO.o:
	$(CC) $(C_FLAGS) -c IO.c

clean:
	rm -f main Main.o IO.o 0 1 2 3 4 5 6 output
