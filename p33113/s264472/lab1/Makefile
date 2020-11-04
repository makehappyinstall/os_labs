GCC=gcc
C_STD=c11
GCC_COMPILE_FLAGS=-Wall -Werror -Wpedantic -std=$(C_STD)
GCC_LINK_FLAGS=-std=$(C_STD) -pthread -lpthread

all: build

main.o: main.c
	$(GCC) $(GCC_COMPILE_FLAGS) -c main.c

build: main.o
	$(GCC) $(GCC_LINK_FLAGS) main.o -o main

clean:
	rm -f main main.o


