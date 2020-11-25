CC=gcc
CC_ARGS=-Wall -Wpedantic -Werror


all: main.o

run: main
	./main

run-silent: main.o
	./main.o 2>/dev/null

main : main.c
	$(CC) $(CC_ARGS) -Wall -Wpedantic -Werror -o main main.c -pthread -lm



# Clean functions
# ---------------

# clean all
clean:
	make cbin cdump

# clean compiled sources
cbin:
	find . -type f -name '*.o' -delete

# clean mememory dumps
cdump:
	find . -type f -name 'dump.*' -delete

