SRC = lab1.c
CC = gcc
DELETE = rm -rf
OUT = lab1.out
LIBS = -lpthread
FLAGS = -Wall -Werror -Wpedantic
all:
	$(CC) $(FLAGS) -o $(OUT) $(SRC) $(LIBS) 
start:
	./$(OUT)
clean:
	$(DELETE) $(OUT) *.txt
