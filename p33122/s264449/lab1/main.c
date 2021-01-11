#include <pthread.h>
#include <math.h>
#include <stdlib.h>
#include <malloc.h>
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

#define DEBUG_THR 0
#define DEBUG_THR_IO 0
#define DEBUG_THR_FILE_IO 0
#define DEBUG_THR_AGG_IO 0
#define DEBUG_THR_NUMBERS 0
#define MEM_ALLOC_ACTION_STOP 0
#define STOP_AT_START 0

#define INFTY_LOOP 1
#define BYTES_IN_MB 1024*1024
#define MEM_SIZE 224 * BYTES_IN_MB
#define START_MEM_CELL_P 0x4B7A441D
#define MEM_THR_AMOUNT 25
#define FILE_SIZE 144 * BYTES_IN_MB
#define FILE_THR_AMOUNT 5
#define IO_BLOCK_SIZE 126
#define IO_BLOCK_ALIGN_SIZE 512
#define AGG_THR_AMOUNT 55
#define RANDOM_FNAME "/dev/urandom"

unsigned char * START_MEM_PTR;


// B=0x4B7A441D;C=mmap;D=25;E=144;F=nocache;G=126;H=random;I=55;J=max;K=futex

typedef struct thread_info  {
    pthread_t thread_id;
    int thread_number;
    unsigned char *start;
    size_t size;
    int fd;
    int block;
} thread_info;

thread_info * mem_thr;
thread_info * file_thr;
thread_info * file_max_thr;


size_t min(size_t a, size_t b) {
    return a < b ? a : b;
}

size_t max(size_t a, size_t b) {
    return a > b ? a : b;
}

int futex_wait(int * ptr, int value) {
    return syscall(SYS_futex,  ptr, FUTEX_WAIT, value, NULL, NULL, 0);
}

int futex_wake(int * ptr, int value) {
    return syscall(SYS_futex,  ptr, FUTEX_WAKE, value, NULL, NULL, 0);
}


void * fillMemBlock(void * args) {
    thread_info * attrs = ((thread_info *) args);
    unsigned char * start = (attrs->start);
    size_t size = (attrs->size);
    int thread_number = (attrs->thread_number);
    int fd = (attrs->fd);

    if (DEBUG_THR) printf("\nTHR %2d MEM start: %p size: %d\n", thread_number, (void *)start, (int)size);

    double min_size = (long)size / (double)IO_BLOCK_SIZE;
    int parts = ceil(min_size);

    if (DEBUG_THR) printf("THR %2d MEM parts: %d min_size: %f\n", thread_number, (int)parts, min_size);

    size_t filled = 0;

    for (int i = 0; i < parts; ++i){
        size_t size_part = min(IO_BLOCK_SIZE, size - filled);
        unsigned char * start_part = start + filled;
        int bytes = read(fd, start_part, size_part);

        if (bytes == -1) {
            if (DEBUG_THR_IO) printf("THR %2d MEM read failed\n", thread_number);
            continue;
        }

        filled += size_part;

        if (DEBUG_THR_IO) printf("THR %2d MEM read part: %d/%d start: %p size_part: %d bytes: %d\n", thread_number, i, parts, (void *) start_part, (int)size_part, bytes);
        if (DEBUG_THR_IO) printf("THR %2d MEM save in memory\n", thread_number);

        if (!DEBUG_THR_NUMBERS) continue;
        printf("THR %2d MEM NUMBERS ", thread_number);
        for (int i = 0; i < size_part; ++i) printf("%d ", start[i] );
        printf("\n");

    }

    if (DEBUG_THR) printf("\nTHR %2d MEM END\n", thread_number);

    pthread_exit(0);
}


void allocateMem() {
    if (DEBUG_THR) printf("TRY ALLOC MEM with mmap ptr: %p\n", (void *) START_MEM_CELL_P);

    START_MEM_PTR = (unsigned char *) mmap((unsigned char *) START_MEM_CELL_P, MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (START_MEM_PTR == MAP_FAILED) {
        printf("ERROR ALLOC MEM with mmap\n");
        exit(-1);
    }

    if (DEBUG_THR || MEM_ALLOC_ACTION_STOP) printf("ALLOC MEM with mmap ptr: %p\n", (void *) START_MEM_PTR);
}


void deallocateMem() {
    if (DEBUG_THR) printf("TRY FREE MEM with ptr: %p\n", (void *) START_MEM_PTR);

    munmap(START_MEM_PTR, MEM_SIZE);
    free(mem_thr);
    free(file_thr);
    free(file_max_thr);
}


void startMemFillThreads() {
    mem_thr = (thread_info *) malloc(sizeof(thread_info) * (MEM_THR_AMOUNT + 1));
    size_t part_size = ceil( (double)MEM_SIZE/MEM_THR_AMOUNT);

    if (DEBUG_THR) printf("MEM_SIZE: %d\n", (int) MEM_SIZE);
    if (DEBUG_THR) printf("MEM_THR_AMOUNT: %d\n", (int) MEM_THR_AMOUNT);
    if (DEBUG_THR) printf("ONE_PART_SIZE: %d\n", (int) part_size);
    if (DEBUG_THR) printf("\n");

    size_t filled_size = 0;
    int fd = open(RANDOM_FNAME, O_RDONLY);


    int i = 0;
    while(filled_size < MEM_SIZE) {
        size_t size = min(part_size, MEM_SIZE - filled_size);

        mem_thr[i].fd = fd;
        mem_thr[i].size = size;
        mem_thr[i].start = START_MEM_PTR + filled_size;
        mem_thr[i].thread_number = i;

        if (DEBUG_THR) printf("Create THR %2d filled_size: %15d size: %d\n",i,  (int) filled_size, (int) size);
        pthread_create(&mem_thr[i].thread_id, NULL, fillMemBlock, &mem_thr[i]);
        i++;
        filled_size += size;

    }
}


void joinMemFillThreads() {
    void * res;

    for (int i = 0; i < MEM_THR_AMOUNT; ++i) {
        pthread_join(mem_thr[i].thread_id, &res);
    }

    close(mem_thr[0].fd);
}


void pauseRun(char * msg) {
    if (!MEM_ALLOC_ACTION_STOP) return;
    printf("%s", msg);
    getchar();
}


void pauseStart(char * msg) {
    if (!STOP_AT_START) return;
    printf("%s", msg);
    getchar();
}


void * fillFile(void * args) {
    thread_info * attrs = ((thread_info *) args);
    int thread_number = (attrs->thread_number);

    if (DEBUG_THR) printf("\nTHR %2d FILE start\n", thread_number);

    char file_name[2];
    sprintf(file_name, "%d", thread_number);
    int fd = open(file_name, O_CREAT | O_TRUNC | __O_DIRECT | O_WRONLY, S_IRWXU | S_IRWXG );

    size_t filled = 0;
    const size_t MAX_OFFSET = MEM_SIZE - IO_BLOCK_SIZE + 1;

    if (DEBUG_THR) printf("THR %2d FILE open: %s fd: %d MAX_OFFSET: %ld\n", thread_number, file_name, fd, MAX_OFFSET);

    while (filled < FILE_SIZE) {
        size_t offset = random() % MAX_OFFSET;
        unsigned char * start = START_MEM_PTR + offset;

        size_t size_part = min(IO_BLOCK_SIZE, FILE_SIZE - filled);
        void * aligned_start = memalign(IO_BLOCK_ALIGN_SIZE, size_part);
        memcpy(aligned_start, start, size_part);

        int bytes = write(fd, aligned_start, IO_BLOCK_ALIGN_SIZE);

        filled += bytes;

        if (bytes == -1) {
            printf("THR %2d FILE write ERROR\n", thread_number);
            continue;
        }

        if (DEBUG_THR_FILE_IO) printf(
            "THR %2d FILE write: %p filed: %10ld / %10d\nsize_part: %ld bytes written: %d offset: %ld\n",
            thread_number, (void *) start, filled, FILE_SIZE, size_part, bytes, offset
        );
    }

    close(fd);

    if (DEBUG_THR) printf("THR %2d FILE END\n", thread_number);

    file_thr[thread_number].block = 1;
    futex_wake(&file_thr[thread_number].block, 1);
    pthread_exit(0);
}


void createFillFilesThreads() {
    file_thr = (thread_info *) malloc(sizeof(thread_info) * FILE_THR_AMOUNT);

    for (int i = 0; i < FILE_THR_AMOUNT; ++i) {
        file_thr[i].thread_number = i;
        file_thr[i].block = 0;
        if (DEBUG_THR) printf("Create THR %2d FILE\n", i);
        pthread_create(&file_thr[i].thread_id, NULL, fillFile, &file_thr[i]);
    }

    for (int i = 0; i < FILE_THR_AMOUNT; ++i) {
       futex_wake(&file_thr[i].block, 1);
    }
}


void wakeFillFilesThreads() {
    for (int i = 0; i < FILE_THR_AMOUNT; ++i) {
        futex_wake(&file_thr[i].block, 1);
    }
}


void joinFillFilesThreads() {
    void * res;

    for (int i = 0; i < FILE_THR_AMOUNT; ++i) {
        pthread_join(file_thr[i].thread_id, &res);
    }
}


unsigned char getMaxFromCharArray(unsigned char * array, size_t size, int debug) {
    unsigned char max_c = array[0];

    if (debug) printf("GET MAX FROM NUMBERS max: %d\n", max_c);

    for (int i = 0; i < size; ++i) {
        if (debug) printf("%5d", array[i]);
        max_c = max((int) max_c, (int) array[i]);
    }
    if (debug) printf("\n");

    return max_c;
}


void * countFileMaxNumber(void * args) {
    thread_info * attrs = ((thread_info *) args);
    int thread_number = (attrs->thread_number);
    int file_number = (attrs->fd);

    futex_wait(&file_thr[file_number].block, 0);

    if (DEBUG_THR) printf("THR %2d MAX for FILE %d start\n", thread_number, file_number);

    char file_name[2];
    sprintf(file_name, "%d", file_number);
    int fd = open(file_name, O_RDONLY);

    unsigned char max_c = 0;
    int bytes;
    unsigned char buffer[IO_BLOCK_SIZE];

    do {
        bytes = read(fd, buffer, IO_BLOCK_SIZE);
        max_c = max(max_c, getMaxFromCharArray(buffer, IO_BLOCK_SIZE, DEBUG_THR_AGG_IO));
        if (DEBUG_THR_AGG_IO) printf("THR %2d MAX for FILE %d read part bytes: %d max: %d\n", thread_number, file_number, bytes, max_c);
    } while (bytes > 0);

    close(fd);

    if (DEBUG_THR) printf("THR %2d MAX for FILE %d END max: %d\n", thread_number, file_number, max_c);

    futex_wake(&file_thr[file_number].block, 1);
    pthread_exit(0);
}


void createMaxCountFilesThreads() {
    file_max_thr = (thread_info *) malloc(sizeof(thread_info) * AGG_THR_AMOUNT);

    for (int i = 0; i < AGG_THR_AMOUNT; ++i) {
        int random_file_number = random() % FILE_THR_AMOUNT;
        file_max_thr[i].thread_number = i;
        file_max_thr[i].fd = random_file_number;

        if (DEBUG_THR) printf("Create THR %2d MAX for FILE %d\n", i, random_file_number);
        pthread_create(&file_max_thr[i].thread_id, NULL, countFileMaxNumber, &file_max_thr[i]);
    }
}


void joinMaxCountFilesThreads() {
    void * res;

    for (int i = 0; i < AGG_THR_AMOUNT; ++i) {
        pthread_join(file_max_thr[i].thread_id, &res);
    }
}


int main(int argc, char const *argv[]) {
    do {
        pauseStart("START (Enter)\n");
        pauseRun("\nBEFORE ALLOC (Enter)\n");
        allocateMem();
        pauseRun("\nAFTER ALLOC (Enter)\n");

        startMemFillThreads();
        joinMemFillThreads();
        pauseRun("\nAFTER MEM FILL (Enter)\n");

        createFillFilesThreads();
        createMaxCountFilesThreads();

        wakeFillFilesThreads();
        joinFillFilesThreads();
        joinMaxCountFilesThreads();

        deallocateMem();
        pauseRun("\nAFTER DEALLOC MEM (Enter)\n");

    } while (INFTY_LOOP);

    return 0;
}