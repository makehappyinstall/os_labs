all: os

os: os.c Makefile
	gcc -pthread $< -Wall -Werror -Wpedantic -o $@

clean:
	$(RM) os

.PHONY: all clean
