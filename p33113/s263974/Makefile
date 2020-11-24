all: run

run: main
	./main

main: main.o
	gcc -pthread -Wall -Werror -Wpedantic -o main *.o

main.o: clean
	gcc -pthread -Wall -Werror -Wpedantic -c *.c

clean:
	rm -f *.o file*.bin main
