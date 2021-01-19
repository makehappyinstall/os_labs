all: run

run: main
	./main

main: main.o
	gcc -pthread -o main *.o

main.o: clean
	gcc -Wall -Werror -Wpedantic -c *.c

clean:
	rm -f *.o file*.bin main
