obj-m:=main.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	sudo insmod main.ko
	sudo mknod /dev/scull c 240 0
	sudo chmod 777 /dev/scull

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	sudo rmmod main.ko
	sudo rm /dev/scull