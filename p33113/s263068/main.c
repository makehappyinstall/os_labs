
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <pthread.h>
#include <limits.h>
#include "threadpool.h"

// A=355;B=0x15ADFE03;C=mmap;D=58;E=116;F=block;G=26;H=random;I=73;J=sum;K=semaphore
#define A 68
#define B 0xDD95BF70
#define C mmap
#define D 120
#define E 51
#define F block
#define G 96
#define H random
#define I 88
#define J max
#define K sem

#define Q(x) #x
#define QUOTE(x) Q(x)

unsigned char max = 0;

typedef struct {
    unsigned char *start_address;
    size_t length;
    FILE *urandom;
} fillerThreadData;

typedef struct {
    const char *filename;
    sem_t *file_semaphores;
} readerThreadData;

typedef struct {
    char filename[256];
    unsigned char result;
    tpool_t *tm;
    sem_t *semaphore;
} readerWorker;

typedef struct {
    const char *filename;
    unsigned char *buffer;
    size_t length;
    size_t offset;
    unsigned char result;
    tpool_t *tm;
} maxAggregateWorker;

void breakPoint() {
    printf("Press ENTER...\n");
    fflush(stdout);

    int c;
    while ((c = getchar()) != 0x0a);
    if (c != EOF) {
        return;
    }

    if (ferror(stdin)) {
        perror("ferror");
        exit(-5);
    }

    if (feof(stdin)) {
        exit(0);
    }
}

void fulfilling(const fillerThreadData data) {
    for (size_t i = 0; i < data.length;) {
        i += fread(data.start_address + i, 1, data.length - i, data.urandom);

        if (ferror(data.urandom)) {
            perror("ferror");
            exit(-3);
        }

        if (feof(data.urandom)) {
            fprintf(stderr, "feof\n");
            exit(-3);
        }
    }

    printf("|");
    fflush(stdout);
}

void *fulfillingThread(const fillerThreadData *data) {
    fulfilling(*data);
    return NULL;
}

void allocate(const char *filename, sem_t *fileSemaphores) {
    breakPoint();

    printf("Allocating %dMiB at the [%p]...\n", A, (void *) B);
    unsigned char *mem = (unsigned char *) C((void *) B, A * 1024 * 1024, PROT_READ | PROT_WRITE,
                                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == (void *) -1) {
        switch (errno) {
            case EACCES:
                fprintf(stderr, "EACCES\n");
                break;

            case EAGAIN:
                fprintf(stderr, "EAGAIN\n");
                break;

            case EBADF:
                fprintf(stderr, "EBADF\n");
                break;

            case EINVAL:
                fprintf(stderr, "EINVAL\n");
                break;

            case ENFILE:
                fprintf(stderr, "ENFILE\n");
                break;

            case ENODEV:
                fprintf(stderr, "ENODEV\n");
                break;

            case ENOMEM:
                fprintf(stderr, "ENOMEM\n");
                break;

            case EOVERFLOW:
                fprintf(stderr, "EOVERFLOW\n");
                break;
        }

        exit(-1);
    }
    breakPoint();

    printf("Fulfilling allocated memory at [%p] with %d threads...\n", mem, D);
    FILE *urandom = fopen("/dev/urandom", "rb");

    if (urandom == NULL) {
        fprintf(stderr, "Unable to open /dev/urandom\n");
        exit(-6);
    }

    pthread_t threads[D - 1];
    fillerThreadData threadsData[D];
    unsigned char *nextAddress = mem;
    unsigned int part = A * 1024 * 1024 / D;
    for (unsigned int i = 0; i < D - 1; ++i) {
        threadsData[i].start_address = nextAddress;
        threadsData[i].length = part;
        threadsData[i].urandom = urandom;

        int err = pthread_create(threads + i, NULL, (void *(*)(void *)) fulfillingThread,
                                 (void *) (threadsData + i));

        switch (err) {
            case EAGAIN:
                fprintf(stderr, "EAGAIN\n");
                exit(-2);

            case EINVAL:
                fprintf(stderr, "EINVAL\n");
                exit(-2);

            case EPERM:
                fprintf(stderr, "EPERM\n");
                exit(-2);
        }

        nextAddress += part;
    }

    threadsData[D - 1].start_address = nextAddress;
    threadsData[D - 1].length = mem + A * 1024 * 1024 - nextAddress;
    threadsData[D - 1].urandom = urandom;
    printf("[");
    fulfilling(threadsData[D - 1]);

    for (unsigned int i = 0; i < D - 1; ++i) {
        int err = pthread_join(threads[i], NULL);

        switch (err) {
            case EDEADLK:
                fprintf(stderr, "EDEADLK\n");
                exit(-4);

            case EINVAL:
                fprintf(stderr, "EINVAL\n");
                exit(-4);

            case ESRCH:
                fprintf(stderr, "ESRCH\n");
                exit(-4);
        }
    }

    int urandom_err = fclose(urandom);
    if (urandom_err == EBADF) {
            fprintf(stderr, "EBADF\n");
            exit(-6);
    }
    printf("] 120/120");
    printf("\n");
    breakPoint();

    printf("Writing to \"%s.n.bin\" %dMiB per file with block access [%d bytes]\n",
           filename, E, G);
    for (unsigned int i = 0, offset = 0; offset < A; ++i, offset += E) {
        char current_file[256];
        int sw = sem_trywait(fileSemaphores + i);
        switch (sw) {
            case EINTR:
                perror("EINTR");
                exit(-9);
            case EINVAL:
                perror("EINVAL");
                exit(-9);
            case EAGAIN:
                perror("EAGAIN");
                exit(-9);
            case ETIMEDOUT:
                perror("ETIMEDOUT");
                exit(-9);
        }

        snprintf(current_file, 256, "%s.%d.bin", filename, i);
        FILE *file = fopen(current_file, "wb");
        size_t fileSize = A - offset;
        if (fileSize > E) {
            fileSize = E;
        }

        fileSize *= 1024 * 1024;
        static unsigned char buf[E * 1024 * 1024];
        memset(buf, 0, fileSize);

        for (unsigned int c = 0; c < fileSize;) {
            c += fwrite(buf + c, 1, fileSize - c, file);
        }

        int seekErr = fseek(file, 0, SEEK_END);
        switch (seekErr) {
            case EBADF:
                fprintf(stderr, "EBADF\n");
                exit(-7);

            case EINVAL:
                fprintf(stderr, "EINVAL\n");
                exit(-7);
        }

        for (size_t blockOffset = 0; blockOffset < fileSize;) {
            size_t count = fileSize - blockOffset;

            if (count > G) {
                count = G;
            }

            seekErr = fseek(file, -count, SEEK_CUR);
            switch (seekErr) {
                case EBADF:
                    fprintf(stderr, "EBADF\n");
                    exit(-7);

                case EINVAL:
                    fprintf(stderr, "EINVAL\n");
                    exit(-7);
            }

            for (size_t blocksWritten = 0; blocksWritten < count;) {
                blocksWritten += fwrite(mem + offset * 1024 * 1024 + fileSize - blockOffset - count + blocksWritten, 1,
                                        count - blocksWritten, file);
            }

            seekErr = fseek(file, -count, SEEK_CUR);
            switch (seekErr) {
                case EBADF:
                    fprintf(stderr, "EBADF\n");
                    exit(-7);

                case EINVAL:
                    fprintf(stderr, "EINVAL\n");
                    exit(-7);
            }

            blockOffset += count;
            printf("%s\t[%lu/%lu bytes]\t%lu%%\t\r", current_file, blockOffset, fileSize, blockOffset/fileSize*100);
        }
        printf("\n");

        if (ferror(stdin)) {
            perror("ferror");
            exit(-8);
        }

        int err = fclose(file);
        if (err == EBADF) {
                fprintf(stderr, "EBADF\n");
                exit(-9);
        }

        sem_post(fileSemaphores + i);
    }
    breakPoint();
    printf("Unmapping...\n");
    munmap(mem, A * 1024 * 1024);
    printf("Unmapping is done\n");
    breakPoint();
}

void maxFinderWorker(void *workData) {
    maxAggregateWorker *data = workData;
    for (size_t i = 0, offset = data->offset; i < data->length; ++i, ++offset) {
        if (data->buffer[offset] > max) {
            max = data->buffer[offset];
            data->result = max;
        }
    }
    printf("%s\tmax: %d\n", data->filename, max);
}

void fileReaderWorker(void *workData) {
    readerWorker *data = workData;

    unsigned char buffer[2048];
    maxAggregateWorker aggregateData = {data->filename, buffer, 2048, 0, 0, data->tm};

    int sw = sem_wait(data->semaphore);
    switch (sw) {
        case EINTR:
            perror("EINTR");
            exit(-9);
        case EINVAL:
            perror("EINVAL");
            exit(-9);
        case EAGAIN:
            perror("EAGAIN");
            exit(-9);
        case ETIMEDOUT:
            perror("ETIMEDOUT");
            exit(-9);
    }

    FILE *file = fopen(data->filename, "rb");
    while (fread(buffer, 1, 2048, file) > 0) {
        tpool_wait_work(tpool_add_work(data->tm, maxFinderWorker, &aggregateData, true));
        if (data->result > max) {
            max = data->result;
        }
    }
    fclose(file);
    sem_post(data->semaphore);
}

_Noreturn void *mainThread(void *threadData) {
    readerThreadData *data = threadData;
    tpool_t *tm = tpool_create(I - 1);
    while (1) {
        const unsigned int filesAmount = A / E + (A % E > 0 ? 1 : 0);

        readerWorker readerWorkerPointer[filesAmount];

        for (unsigned int i = 0; i < filesAmount; ++i) {
            snprintf(readerWorkerPointer[i].filename, 256, "%s.%d.bin", data->filename, i);
            readerWorkerPointer[i].result = 0;
            readerWorkerPointer[i].tm = tm;
            readerWorkerPointer[i].semaphore = data->file_semaphores + i;
            tpool_add_work(tm, fileReaderWorker, readerWorkerPointer + i, false);
        }
        tpool_wait(tm);
    }
}

int main() {
    srand(time(NULL));
    char filename[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    for (unsigned int i = 0; i < 9; ++i) {
        filename[i] = random() % ('z' - 'A') + 'A';
    }

    printf("Filename is \"%s\"\n", filename);


    unsigned int file_semaphores_count = A / E + 1;
    sem_t file_semaphores[file_semaphores_count];

    for (unsigned int i = 0; i < file_semaphores_count; ++i) {
        sem_init(file_semaphores + i, 0, 1);
    }

    allocate(filename, file_semaphores);

    pthread_t read_thread;
    readerThreadData read_thread_data = {filename, file_semaphores};
    pthread_create(&read_thread, NULL, mainThread, &read_thread_data);

    while (1) {
        allocate(filename, file_semaphores);
        pthread_t read_thread;
        readerThreadData read_thread_data = {filename, file_semaphores};
        pthread_create(&read_thread, NULL, mainThread, &read_thread_data);
    }

    return 0;
}