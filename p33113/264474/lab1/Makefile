all: main

main: main.c
gcc -c main.c FileIO.c MemoryFiller.c
gcc -pthread -o main -Wall -Werror -Wpedantic main.o FileIO.o MemoryFiller.o -lm

clean:
rm -f main *.o