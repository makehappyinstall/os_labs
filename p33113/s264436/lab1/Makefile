CC := gcc
FLAGS := -pedantic-errors -Wall -Werror -pthread

all: clean build run

build: main.c
	$(CC) $(FLAGS) -o main main.c

run:
	./main

clean:
	rm -f main *.dat
