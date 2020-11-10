SRC = lab1.c
CC = gcc
DELETE = rm -rf
OUT = lab1.out
THREADFLAG = -lpthread
FLAGS = -Wall -Werror -Wpedantic
all:
	$(CC) $(FLAGS) -o $(OUT) $(SRC) $(THREADFLAG) 
	./$(OUT)
clear:
	$(DELETE) $(OUT) *.txt
