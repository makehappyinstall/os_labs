CC = gcc
DESTDIR = ./build
DEST = $(DESTDIR)/main
SRC = src/main.c
LIBF = ./lib/
LIB = -lcflags -lpthread
FLAGS = -Wall -Werror -Wpedantic

all: src/main.c
	mkdir $(DESTDIR)
	$(CC) $(FLAGS) -o $(DEST) $(SRC) -L$(LIBF) $(LIB)

debug: src/main.c
	mkdir $(DESTDIR)
	$(CC) -g -o $(DEST) $(SRC) -L$(LIBF) $(LIB)

clean:
	rm -rf $(DESTDIR)
