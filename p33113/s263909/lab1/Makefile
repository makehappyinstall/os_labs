CC = gcc
DESTDIR = ./build
DEST = $(DESTDIR)/main
SRC = src/main.c
LIBF = ./lib/
LIB = -lcflags -lpthread
FLAGS = -Wall -Werror -Wpedantic

all: src/main.c
	$(CC) $(FLAGS) -o $(DEST) $(SRC) -L$(LIBF) $(LIB)

debug: src/main.c
	$(CC) -g -o $(DEST) $(SRC) -L$(LIBF) $(LIB)

dirs:
	mkdir $(DESTDIR)

clean:
	rm -rf $(DESTDIR)
