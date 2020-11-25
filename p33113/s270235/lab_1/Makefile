SRC = main.c
CC = gcc
OUTPUT = main
LIBS = -lpthread
FLAGS = -std=c99 -D_GNU_SOURCE -Wall -Werror -Wpedantic

all: 
	$(CC) $(FLAGS) -o $(OUTPUT) $(SRC) $(LIBS) 


clean:
	rm -f main *.o res/*