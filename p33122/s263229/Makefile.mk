SRC = lab1.c
CC = gcc
DELETE = rm -rf
OUT = lab1.out
LIBS = -lpthread
all:
	$(CC) -o $(OUT) $(SRC) $(LIBS)
clean:
	$(DELETE) $(OUT) *.txt