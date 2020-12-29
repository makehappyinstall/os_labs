#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define DEVICE "/dev/genius"
#define BUFF_SIZE 100

int main() {
    int fd;
    char read_buf[BUFF_SIZE];

    fd = open(DEVICE, O_RDONLY);

    if (fd == -1) {
	    fprintf(stderr, "Can't open the %s!\n", DEVICE);
        exit(-1);
    }
    read(fd, &read_buf, BUFF_SIZE);
    printf("%s\n", read_buf);
    exit(0);
}
