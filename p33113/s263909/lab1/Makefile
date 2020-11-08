CC = gcc
DEST = build/main
SRC = src/main.c
LIBF = ./lib/
LIB = -lcflags -lpthread
FLAGS = -Wall -Wpedantic

all: src/main.c
	$(CC) $(FLAGS) -o $(DEST) $(SRC) -L$(LIBF) $(LIB)

debug: src/main.c
	$(CC) -g -o $(DEST) $(SRC) -L$(LIBF) $(LIB)

clean:
	rm $(DEST) res/*
