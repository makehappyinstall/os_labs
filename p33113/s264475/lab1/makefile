C=gcc
C_STD=c11
C_FLAGS= -Werror -Wpedantic -Wall
LIBS=-pthread -lm

all: clean main

main: main.o
	$(C) $(C_FLAGS) $(LIBS) main.o -o main

main.o:
	$(C) $(C_FLAGS) -c main.c

clean:
	rm -f main Main.o os_file_* output