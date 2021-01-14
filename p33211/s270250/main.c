#include <stdio.h>
#include <sys/mman.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>

//A=200;B=0x1D284B2F;C=mmap;D=25;E=46;F=nocache;G=27;H=seq;I=20;J=sum;K=cv
#define ALLOCATE_MEMORY 200 // A
#define ADDRESS 0x1D284B2F // B
//#define mmap // C
#define GENERATED_THREADS_AMOUNT 25 // D
#define FILE_SIZE 46 // E
//#define nocache //F
#define BLOCK_SIZE_BYTES 27 // G
//#define seq // H
#define READ_THREADS_AMOUNT 20 // I
//#define sum // J
//#define cv // K

#define IN_BYTES (1024 * 1024)

#define INTEGER_GENERATED ((ALLOCATE_MEMORY * IN_BYTES) / GENERATED_THREADS_AMOUNT)
#define REMAINDER_GENERATED ((ALLOCATE_MEMORY * IN_BYTES) % GENERATED_THREADS_AMOUNT)

#define COUNT_FILES (ALLOCATE_MEMORY / FILE_SIZE)

#define URANDOM "/dev/urandom"

uint8_t *address;
FILE **files;
pthread_mutex_t mutexes[COUNT_FILES];
pthread_cond_t conds[COUNT_FILES];
int check[COUNT_FILES];

struct GenDataThread {
    size_t start;
    size_t end;
    FILE *urandom;
};

struct WritableDataThread {
    int threadId;
    int sizeMemory;
};

struct ReadableDataThread {
    int fileId;
    int threadId;
};

void generateInMemory();
void *writeInMemory(void *);
void writeInFiles();
void *writeInFile(void *);
void createFiles();
void readFromFiles();
_Noreturn void *readFromFile(void *);

int main() {

    printf("Allocate memory. Press a key to start");
    getchar();

    address = mmap(
            (void*) ADDRESS,
            ALLOCATE_MEMORY * IN_BYTES,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1, 0);

    if (address == MAP_FAILED) {
        perror("Error address a file");
        exit(EXIT_FAILURE);
    }

    printf("Write data to memory. Press a key to start");
    getchar();

    generateInMemory();

    printf("Deallocate memory. Press a key to start");
    getchar();

    munmap(address, ALLOCATE_MEMORY * IN_BYTES);

    printf("Infinity writing and reading. Press a key to start");
    getchar();


    int firstRun = 1;

    createFiles();

    while (1) {

        address = mmap(
                (void*) ADDRESS,
                ALLOCATE_MEMORY * IN_BYTES,
                PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS,
                -1, 0);
        if (address == MAP_FAILED) {
            perror("Error address a file");
            exit(EXIT_FAILURE);
        }

        generateInMemory();
        writeInFiles();

        if (firstRun) {
            readFromFiles();
            firstRun = 0;
        }
    }

    return 0;
}

void generateInMemory() {
    FILE *urandom = fopen(URANDOM, "r");
    pthread_t memoryThreads[GENERATED_THREADS_AMOUNT];
    struct GenDataThread *genDataThread = (struct GenDataThread *) malloc(GENERATED_THREADS_AMOUNT * sizeof(struct GenDataThread));
    for (int i = 0; i < GENERATED_THREADS_AMOUNT - 1; i++) {
        genDataThread[i].start = (size_t) i * INTEGER_GENERATED;
        genDataThread[i].end = (size_t) (i + 1) * INTEGER_GENERATED;
        genDataThread[i].urandom = urandom;

        pthread_create(&(memoryThreads[i]), NULL, writeInMemory, &(genDataThread[i]));
    }

    genDataThread[GENERATED_THREADS_AMOUNT - 1].start = (size_t) (GENERATED_THREADS_AMOUNT - 1) * INTEGER_GENERATED;
    genDataThread[GENERATED_THREADS_AMOUNT - 1].end = (size_t) (GENERATED_THREADS_AMOUNT - 1) * INTEGER_GENERATED + REMAINDER_GENERATED;
    genDataThread[GENERATED_THREADS_AMOUNT - 1].urandom = urandom;

    pthread_create(&(memoryThreads[GENERATED_THREADS_AMOUNT - 1]),
                   NULL,
                   writeInMemory,
                   &(genDataThread[GENERATED_THREADS_AMOUNT - 1]));

    for (int i = 0; i < GENERATED_THREADS_AMOUNT; i++) {
        pthread_join(memoryThreads[i], NULL);
    }

    fclose(urandom);
}

void *writeInMemory(void *dataThread) {
    struct GenDataThread *genDataThread = (struct GenDataThread *) dataThread;
    for (size_t i = genDataThread->start; i < genDataThread->end; i++) {
        uint8_t number = 0;
        fread(&number, 1, 1, genDataThread->urandom);
        address[i] = number;
    }
    return NULL;
}

void writeInFiles() {
    pthread_t *filesThreads = malloc(COUNT_FILES * sizeof(pthread_t));
    struct WritableDataThread *writableDataThread = (struct WritableDataThread *) malloc(COUNT_FILES * sizeof(struct WritableDataThread));
    for (int i = 0; i < COUNT_FILES; i++) {
        writableDataThread[i].threadId = i;
        writableDataThread[i].sizeMemory = ((i + 1) * FILE_SIZE) < ALLOCATE_MEMORY
                ? FILE_SIZE * IN_BYTES
                : (ALLOCATE_MEMORY - i * FILE_SIZE) * IN_BYTES;

        pthread_create(&(filesThreads[i]), NULL, writeInFile, &(writableDataThread[i]));
    }

    for (int i = 0; i < COUNT_FILES; i++) {
        pthread_join(filesThreads[i], NULL);
    }

    free(filesThreads);
}

void *writeInFile(void *dataThread) {
    struct WritableDataThread *writableDataThread = (struct WritableDataThread *) dataThread;
    int threadId = writableDataThread->threadId;
    int sizeMemory = writableDataThread->sizeMemory;
    pthread_mutex_t *mutex = &(mutexes[threadId]);
    pthread_cond_t *cond = &(conds[threadId]);
    FILE *file = files[threadId];

    fseek(file, 0, SEEK_SET);
    size_t offsetMinor = threadId * FILE_SIZE * IN_BYTES;
    for (int i = 0; i < sizeMemory; i += BLOCK_SIZE_BYTES) {
        pthread_mutex_lock(mutex);
        check[threadId] = 1;

        int nelts = (i + BLOCK_SIZE_BYTES) < sizeMemory? BLOCK_SIZE_BYTES: (sizeMemory - i);
        size_t offsetMajor = offsetMinor + i;
        fwrite((address + offsetMajor), 1, nelts, file);
        fflush(file);

        check[threadId] = 0;
        pthread_cond_signal(cond);
        pthread_mutex_unlock(mutex);
    }

    return NULL;
}

void createFiles() {
    files = (FILE **) malloc(COUNT_FILES * sizeof(FILE *));
    char filename[20];
    for (int i = 0; i < COUNT_FILES; i++) {
        sprintf(filename, "file%d", i);
        files[i] = fopen(filename, "w+b");

        pthread_mutex_init(&(mutexes[i]), NULL);
        pthread_cond_init(&(conds[i]), NULL);
    }
}

void readFromFiles() {
    pthread_t *readFiles = (pthread_t *) malloc(READ_THREADS_AMOUNT * sizeof(pthread_t));
    struct ReadableDataThread *readableDataThread = (struct ReadableDataThread *) malloc(READ_THREADS_AMOUNT * sizeof(struct ReadableDataThread));
    for (int i = 0; i < READ_THREADS_AMOUNT; i++) {
        readableDataThread[i].threadId = i;
        readableDataThread[i].fileId = i % COUNT_FILES;

        pthread_create(&(readFiles[i]), NULL, readFromFile, &(readableDataThread[i]));
    }

}

_Noreturn void *readFromFile(void *dataThread) {
    struct ReadableDataThread *readableDataThread = (struct ReadableDataThread *) dataThread;
    int fileId = readableDataThread->fileId;
    pthread_mutex_t *mutex = &(mutexes[fileId]);
    pthread_cond_t *cond = &(conds[fileId]);
    FILE *file = files[fileId];

    while (1) {
        fseek(file, 0, SEEK_SET);
        unsigned long long sum = 0;
        for (int i = 0; i < FILE_SIZE * IN_BYTES; i++) {
            pthread_mutex_lock(mutex);

            while (check[fileId]) {
                pthread_cond_wait(cond, mutex);
            }

            uint8_t number;
            fread(&number, 1, 1, file);
            sum += number;

            pthread_mutex_unlock(mutex);
        }
        printf("Sum in file %d with thread %d is equal %llu\n", fileId, readableDataThread->threadId,sum);
    }
}