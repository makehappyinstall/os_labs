#include <stdio.h>
#include <sys/mman.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "limits.h"

#define B 0xED930667
#define A 232//232
#define D 20 //threads
#define E 116 //file sizes in MB
#define G 95 //IO block size
#define I 141 //reader file threads

typedef struct {
    char *adr;
    size_t bytes;
    char *randomnums;
} MemoryFillerArgs;


typedef struct {
    pthread_mutex_t *mutex;
    char *adr;
    char *filename;
} FileFillerArgs;

typedef struct {
    pthread_mutex_t *mutex;
    char *filename;
} FileReaderArgs;

void *memoryFiller(void *arg) {
    while (1) {
        MemoryFillerArgs *forthread_ptr = (MemoryFillerArgs *) arg;
        FILE *fdr = fopen(forthread_ptr->randomnums, "rb");
        size_t i = 0;
        while (i < forthread_ptr->bytes) {
            i += fread(forthread_ptr->adr + i, 1, forthread_ptr->bytes - i, fdr);
        }
        fclose(fdr);
        return NULL;
    }
}

_Noreturn void *fileFiller(void *arg) {
    while (1) {
        FileFillerArgs *args = (FileFillerArgs *) arg;
        char *filename = args->filename;
        char *src = args->adr;
        pthread_mutex_t *mutex = args->mutex;
        pthread_mutex_lock(mutex);
        FILE *fd = fopen(filename, "wb");
        lockf(fileno(fd), F_LOCK, 0);
        fseek(fd, 0, SEEK_SET);
        int fileBlockCount = E * 1024 * 1024 / G;
        for (int i = 0; i < fileBlockCount; i++) {
            fwrite(src, 1, G, fd);
        }
        if (E * 1024 * 1024 % G != 0) {
            fwrite(src, 1, E * 1024 * 1024 % G, fd);
        }
        printf("Data for %s file generated\n", filename);
        lockf(fileno(fd), F_ULOCK, 0);
        fclose(fd);
        pthread_mutex_unlock(mutex);
    }
}


int find_max(int *buf, int size) {
    int max = INT_MIN;
    for (int *i = buf; i < buf + size; i++) {
        int val = *i;
        if (val > max)
            max = val;
    }
    return max;
}


_Noreturn void *fileReader(void *arg) {
    while (1) {
        FileReaderArgs *args = (FileReaderArgs *) arg;
        char *filename = args->filename;
        pthread_mutex_t *mutex = args->mutex;
        pthread_mutex_lock(mutex);
        FILE *fd = fopen(filename, "rb");
        if (fd == NULL) {
            pthread_mutex_unlock(mutex);
            continue;
        }
        lockf(fileno(fd), F_LOCK, 0);
        fseek(fd, 0, SEEK_SET);
        int fileBlockCount = E * 1024 * 1024 / G;
        int max = INT_MIN;
        char *buf[G];
        for (int i = 0; i < fileBlockCount; i++) {
            if (fread(buf, 1, G, fd) != G) {
                continue;
            }
            int localMax = find_max((int *) buf, G / 4);
            if (localMax > max) {
                max = localMax;
            }
        }
        printf("Maximum in %s file = %d\n", filename, max);
        lockf(fileno(fd), F_ULOCK, 0);
        fclose(fd);
        pthread_mutex_unlock(mutex);
    }
}

int main() {
    printf("Enter desired running time in seconds: ");
    int time;
    scanf("%d", &time);
    //mapping A megabytes from B address (if possible) using mmap
    char *startAddress = mmap((void *) B, A * 1024 * 1024, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (startAddress == MAP_FAILED) {
        printf("Bro, we failed");
        return 0;
    } else {
        printf("Starting address of mapped area is %p\n", startAddress);
        getchar();
        getchar();
    }
    //filling our mapped area with random numbers in D threads
    char *randomnums = "/dev/urandom";
    long block = A * pow(2, 20) / D;
    MemoryFillerArgs memoryFillerArgs[D];
    pthread_t memoryFillerThreads[D - 1];
    for (int i = 0; i < D - 1; i++) {
        memoryFillerArgs[i].adr = startAddress;
        memoryFillerArgs[i].bytes = block;
        memoryFillerArgs[i].randomnums = randomnums;
        pthread_create(&memoryFillerThreads[i], NULL, memoryFiller, &memoryFillerArgs[i]);
        startAddress += block;
    }
    //memoryFillerThreads number D
    memoryFillerArgs[D - 1].adr = startAddress;
    memoryFillerArgs[D - 1].bytes = block + (int) (A * pow(2, 20)) % D;
    memoryFillerArgs[D - 1].randomnums = randomnums;
    pthread_create(&memoryFillerThreads[D - 1], NULL, memoryFiller, &memoryFillerArgs[D - 1]);
    startAddress -= block * (D - 1);

    int fileCnt = A / E;
    pthread_mutex_t mutexes[fileCnt];
    for (int i = 0; i < fileCnt; i++) {
        pthread_mutex_init(&(mutexes[i]), NULL);
    }
    
    //files writing threads
    FileFillerArgs fileFillerArgs[fileCnt];
    pthread_t fileFillerThreads[fileCnt];
    for (int i = 0; i < fileCnt; i++) {
        char *filename = malloc(sizeof(char) * 7);
        snprintf(filename, 7, "file_%d", i);
        fileFillerArgs[i].filename = filename;
        fileFillerArgs[i].adr = startAddress;
        fileFillerArgs[i].mutex = &(mutexes[i]);
        pthread_create(&fileFillerThreads[i], NULL, fileFiller, &fileFillerArgs[i]);
    }

    //files reading threads
    FileReaderArgs fileReaderArgs[I];
    pthread_t fileReaderThreads[I];
    int k = 0;
    for (int i = 0; i < I; i++) {
        char *filename = malloc(sizeof(char) * 7);
        snprintf(filename, 7, "file_%d", k);
        fileReaderArgs[i].filename = filename;
        fileReaderArgs[i].mutex = &(mutexes[k]);
        pthread_create(&fileReaderThreads[i], NULL, fileReader, &fileReaderArgs[i]);
        k++;
        if (k == fileCnt)
            k = 0;
    }
    sleep(time);
    for (int i = 0; i < D; ++i) {
        pthread_cancel(memoryFillerThreads[i]);
    }
    for (int i = 0; i < fileCnt; ++i) {
        pthread_cancel(fileFillerThreads[i]);      	
    }
    for (int i = 0; i < I; ++i) {
        pthread_cancel(fileReaderThreads[i]);
    }
    getchar();
    printf("Memory is ready to be deallocated");
    getchar();
    getchar();
    //unmapping our area of A MB
    munmap(startAddress, A * pow(2, 20));
    printf("Memory was deallocated");
    getchar();
    getchar();
    return 0;
}
