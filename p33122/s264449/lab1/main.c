#include <pthread.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <stdatomic.h>
#include <linux/futex.h>
#include <syscall.h>
#include <string.h>

#define DEBUG_THR 1
#define INFTY_LOOP 0
#define BYTES_IN_MB 1024*1024
#define MEM_SIZE 224
#define START_MEM_CELL_P 0x4B7A441D
#define MEM_THR_AMOUNT 25
#define FILE_SIZE 144
#define IO_BLOCK_SIZE 126
#define AGG_THR_AMOUNT 55
#define RANDOM_FNAME "/dev/urandom"
#define FILES_AMOUNT ceil(MEM_SIZE / FILE_SIZE)

int * START_MEM_PTR;


// B=0x4B7A441D;C=mmap;D=25;E=144;F=nocache;G=126;H=random;I=55;J=max;K=futex

size_t min(size_t a, size_t b) {
    return a < b ? a : b;
}

typedef struct fill_mem_thread_info  {
    pthread_t thread_id;
    int thread_number;
    int *start;
    size_t size;
    size_t offset;
    int fd;
} fill_mem_thread_info;

void* fillMemBlock(void * args) {
    fill_mem_thread_info * attrs = ((fill_mem_thread_info *) args);
    int * start = (attrs->start);
    size_t size = (attrs->size);
    size_t offset = (attrs->offset);
    int thread_number = (attrs->thread_number);
    int fd = (attrs->fd);

    if (DEBUG_THR) printf("THR %d MEM start: %p size: %d\n", thread_number, (void *)start, (int)size);

    double min_size = (long)size / (double)IO_BLOCK_SIZE;
    int parts = ceil(min_size);
    if (DEBUG_THR) printf("THR %d MEM parts: %d min_size: %f\n", thread_number, (int)parts, min_size);


    // int * buf = (int *) mmap(0, IO_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    // if (DEBUG_THR) printf("THR MEM alloc buff: %p\n", (void *)buf);
    size_t  filled = 0;

    for (int i = 0; i < parts; ++i){
        size_t size_part = min(IO_BLOCK_SIZE, size - filled);
        int bytes = read(fd, attrs->start, size_part);

        if (bytes == -1) {
            if (DEBUG_THR) printf("THR %d MEM read failed\n", thread_number);
            continue;
        }

        if (DEBUG_THR) printf("THR %d MEM read fd: %d size_part: %d bytes: %d\n", thread_number, fd, (int)size_part, bytes);

        // *start = *(buf);

        if (DEBUG_THR) printf("THR %d MEM save in memory\n", thread_number);

        printf("THR MEM NUMBERS ");
        for (int i = 0; i < size_part; ++i)
        {
            printf("%d ", start[i] );
        }
            printf("\n");

        filled += size_part;
    }


    mmap(start, size, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, offset);
    pthread_exit(0);
}

void fillMem() {
    START_MEM_PTR = (int *) mmap((void *) 0x4B7A441D, MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (START_MEM_PTR == MAP_FAILED) {
        printf("ERROR ALLOC MEM with mmap\n");
        exit(-1);
    }

    if (DEBUG_THR) printf("ALLOC MEM with mmap ptr: %p\n", (void*)START_MEM_PTR);

    fill_mem_thread_info * mem_thr = (fill_mem_thread_info *) malloc(sizeof(mem_thr) * MEM_THR_AMOUNT);
    void * res;

    size_t part_size = ceil(MEM_SIZE/MEM_THR_AMOUNT);
    size_t filled_size = 0;
    int fd = open(RANDOM_FNAME, O_RDONLY);

    // FillMemAttrs attrs;

    int i = 0;
    while(filled_size < MEM_SIZE) {
        size_t size = min(part_size, MEM_SIZE - filled_size);

        mem_thr[i].fd = fd;
        mem_thr[i].size = size;
        mem_thr[i].offset = 0;
        mem_thr[i].start = START_MEM_PTR + filled_size;
        mem_thr[i].thread_number = i;


        if (DEBUG_THR) printf("Create thr %d filled_size: %d size: %d\n",i,  (int)filled_size, (int) size);
        pthread_create(&mem_thr[i].thread_id, NULL, fillMemBlock, &mem_thr[i]);
        i++;
        filled_size += size;
        pthread_join(mem_thr[i].thread_id, &res);

    }

    // for (int i = 0; i < MEM_THR_AMOUNT; ++i)
    // {
    //     pthread_join(mem_thr[i].thread_id, &res);
    //     /* code */
    // }
}

int main(int argc, char const *argv[])
{
    fillMem();
    return 0;
}