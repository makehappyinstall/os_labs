CC := gcc
FLAGS := -pedantic-errors -Wall -Werror -pthread -o

all: clean build run

build: main.c
	$(CC) $(FLAGS) main main.c

run:
	./main

clean:
	rm -f main *.dat
