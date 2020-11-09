SRC = lab1.c
CC = gcc
DELETE = rm -rf
OUT = a.out
THREADFLAG = -lpthread
all:
	$(CC) -g -Wall -Werror -Wpedantic -O0 $(SRC) $(THREADFLAG) 
	./$(OUT)
clear:
	$(DELETE) $(OUT) *.txt
