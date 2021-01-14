CC=gcc
C_STD=c11
C_FLAGS=-Wall -Werror -Wpedantic -std=$(C_STD)
LIBS=std=$(C_STD) -pthread -lpthread

all: build

main.o: main.c
	$(CC) $(C_FLAGS) -c main.c

build: main.o
	$(CC) $(C_LIBS) main.o -o main

clean:
	rm -f main main.o
