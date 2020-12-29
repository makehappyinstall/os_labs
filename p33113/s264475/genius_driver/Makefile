obj-m:=genius.o
genius-objs:=driver/registration_and_io.o driver/genius_driver.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules

clean:
	rm -rf test/main;
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean

load:
	sudo insmod genius.ko

get_major:
	cat /proc/devices | grep -e $(NAME)
#	or: dmesg | grep -e $(NAME)

make_driver:
	sudo mknod /dev/$(NAME) c $(MAJOR) 0
	sudo chmod 777 /dev/$(NAME)


unload:
	sudo rmmod genius.ko

test: compile-test
	./test/main

compile-test: main.o
	gcc main.o -o test/main

main.o:
	gcc -c test/main.c
