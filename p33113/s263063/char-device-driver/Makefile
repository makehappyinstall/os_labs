obj-m:=kekdriver.o
kekdriver-objs:=driver/device_reg.o driver/kek_driver.o driver/device_callbacks.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules

clean:
	rm -rf test/main;
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean

load:
	sudo insmod kekdriver.ko

unload:
	sudo rmmod kekdriver.ko

test: compile-test
	./test/main

compile-test: main.o
	gcc main.o -o test/main

main.o:
	gcc -c test/main.c