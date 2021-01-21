#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define DEVICE "/dev/scull"
#define BUFF_SIZE 100

int main()
{
        int fd;
        char ch, write_buf[BUFF_SIZE], read_buf[BUFF_SIZE];

        fd = open(DEVICE, O_RDWR);

        if (!fd)
                return -1;

        printf("r - read, w - write\n");
        scanf("%c", &ch);

        switch (ch) {
                case 'w':
                        printf("enter data: ");
                        scanf(" %[^\n]", write_buf);
                        write(fd, write_buf, sizeof(write_buf));
                        break;
                case 'r':
                        read(fd, read_buf, sizeof(read_buf));
                        printf("scull: %s\n", read_buf);
                        break;
        }

        return 0;
}
