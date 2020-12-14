#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define DEVICE "/dev/kek"
#define BUFF_SIZE 100

int main() {
    int fd;
    char read_buf[BUFF_SIZE];

    fd = open(DEVICE, O_RDWR);

    if (fd == -1) {
	fprintf(stderr, "Can't open the device %s!\n", DEVICE);
        return -1;
    }

    read(fd, &read_buf, BUFF_SIZE);
    printf("%s\n", read_buf);
    return 0;
}
