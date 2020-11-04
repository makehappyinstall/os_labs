#define _XOPEN_SOURCE 500
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdint.h>

#include <search.h>
#include <time.h>

#define checkMalloc(memory, err) if(memory == NULL){ printf("%s is not allocated", err); exit(1); }

#define A 262
// #define B 0xACDC45F1
// #define C malloc
#define D 113
#define E 26
#define G 113
// #define H random
#define I 129
// #define J sum
// #define K cv

#define RANDOM "/dev/urandom"
#define RANDOM_MODE "r"

#define MB_TO_BYTE 1024*1024
#define A_BYTE A*MB_TO_BYTE // 274726912
#define E_BYTE E*MB_TO_BYTE

#define COUNT_BYTE_TO_WRITE A_BYTE/D
#define COUNT_FILE A/E

#define BLOCK_SIZE 4096
#define BLOCK_COUNT E_BYTE / BLOCK_SIZE

#define FILENAME "file_%d.bin"
#define FLAG_TO_CREATE_FILE O_RDWR | O_CREAT | O_FSYNC

typedef struct {
    size_t start;
    size_t stop;
} genDataThread;

typedef struct {
    int number;
} writeFileData;

typedef struct {
    int number;
    int fileNumber;
} readFromFileData;

typedef struct {
    int number;
    size_t start;
    size_t end;
} block;

pthread_mutex_t mutexs[COUNT_FILE];
pthread_cond_t conds[COUNT_FILE];
size_t isWrite[COUNT_FILE] = {0};

block MBlockRead[BLOCK_COUNT];
block MBlockWrite[BLOCK_COUNT];

int *files;
int fp;
uint8_t *memory;

void generateRandomData();

void writeToFile();

void readFromFile();

void shuffle(block *arr, int N) {
    srand(time(NULL));
    for (int i = N - 1; i >= 1; i--) {
        int j = rand() % (i + 1);
        block tmp = arr[j];
        arr[j] = arr[i];
        arr[i] = tmp;
    }
}

int main() {
    short firstRun = 1;

    for (int i = 0; i < BLOCK_COUNT; i++) {
        MBlockWrite[i].number = i;
        MBlockWrite[i].start = i * BLOCK_SIZE;
        MBlockWrite[i].end = (i + 1) * BLOCK_SIZE;

        MBlockRead[i].number = i;
        MBlockRead[i].start = i * BLOCK_SIZE;
        MBlockRead[i].end = (i + 1) * BLOCK_SIZE;
    }

    shuffle(MBlockRead, BLOCK_COUNT);

    files = (int*)malloc(COUNT_FILE * sizeof(int));
    char filename[35];
    for (int i = 0; i < COUNT_FILE; i++) {
        snprintf(filename, 35, FILENAME, i);
        files[i] = open(filename, FLAG_TO_CREATE_FILE);
        pthread_cond_init(&(conds[i]), NULL);
        pthread_mutex_init(&(mutexs[i]), NULL);
    }

    fp = open(RANDOM, O_RDONLY);

    while (1) {
        memory = (uint8_t *) malloc(A_BYTE);
        checkMalloc(memory, "Memory")

        generateRandomData();

        shuffle(MBlockWrite, BLOCK_COUNT);
        writeToFile();

        if (firstRun) {
            firstRun = 0;
            readFromFile();
        }

        free(memory);
    }
//    return 0;
}

void *threadRandomGenerate(void *thread_data) {
    genDataThread *data = (genDataThread *) thread_data;
//    FILE *fp = fopen(RANDOM, RANDOM_MODE);
    for (size_t i = data->start; i < data->stop; i++) {
        uint8_t random = 0;
        read(fp, &random, 1);
//        fread(&random, sizeof(uint8_t), 1, fp);
        memory[i] = random;
    }
//    fclose(fp);
    return NULL;
}

void generateRandomData() {
    pthread_t *threads = (pthread_t *) malloc(D * sizeof(pthread_t));
    genDataThread *threadData = (genDataThread *) malloc(D * sizeof(genDataThread));

    checkMalloc(threads, "Thread's Generate")
    checkMalloc(threadData, "ThreadData Generate")

    for (size_t i = 0; i < D - 1; i++) {
        threadData[i].start = i * COUNT_BYTE_TO_WRITE;
        threadData[i].stop = (i + 1) * COUNT_BYTE_TO_WRITE;
    }

    for (int i = 0; i < D; i++)
        pthread_create(&(threads[i]), NULL, threadRandomGenerate, &threadData[i]);

    for (int i = 0; i < D; i++)
        pthread_join(threads[i], NULL);

    free(threads);
//    free(threadData);
}

void *threadWriteToFile(void *thread_data) {
    writeFileData *data = (writeFileData *) thread_data;
    int thread = data->number;
    pthread_mutex_t *mutex = &(mutexs[thread]);
    pthread_cond_t *cond = &conds[thread];
    int toWrite = files[thread];

    if (toWrite == -1) {
        printf("Creat or write file is have error.\n");
        exit(1);
    }

    int offset = thread * E_BYTE;
    for (int j = 0; j < BLOCK_COUNT; j++) {
        size_t start = MBlockWrite[j].start;
        size_t end = MBlockWrite[j].end;

        pthread_mutex_lock(mutex);
        isWrite[thread] = 1;

        for (size_t k = start; k < end; k += G) {
            uint8_t arrToWrite[G];
            for (int h = 0; h < G; ++h) {
                arrToWrite[h] = memory[offset + k + h];
            }
            pwrite(toWrite, arrToWrite, G, k);
            fsync(toWrite);
        }

        isWrite[thread] = 0;
        pthread_cond_signal(cond);
        pthread_mutex_unlock(mutex);

    }

    return NULL;
}

void writeToFile() {
    pthread_t *threads = (pthread_t *) malloc(COUNT_FILE * sizeof(pthread_t));
    writeFileData *threadData = (writeFileData *) malloc(COUNT_FILE * sizeof(writeFileData));

    checkMalloc(threads, "Thread's Write")
    checkMalloc(threadData, "ThreadData Write")

    for (int i = 0; i < COUNT_FILE; ++i) {
        threadData[i].number = i;
    }

    for (int i = 0; i < COUNT_FILE; ++i) {
        pthread_create(&(threads[i]), NULL, threadWriteToFile, &threadData[i]);
    }

    for (int i = 0; i < COUNT_FILE; i++)
        pthread_join(threads[i], NULL);

    free(threads);
    free(threadData);
}

void *readFileThread(void *thread_data) {
    readFromFileData *data = (readFromFileData *) thread_data;

    int fileNumber = data->fileNumber;
    pthread_mutex_t *mutex = &(mutexs[fileNumber]);
    pthread_cond_t *cond = &(conds[fileNumber]);
    int fp = files[fileNumber];

    while (1) {
        unsigned long long sum = 0;
        for (int i = 0; i < BLOCK_COUNT; i++) {
            size_t start = MBlockRead[i].start;
            size_t end = MBlockRead[i].end;

            pthread_mutex_lock(mutex);
            while (isWrite[fileNumber]) {
                pthread_cond_wait(cond, mutex);
            }

            lseek(fp, start, SEEK_SET);
            for (size_t j = start; j < end; j++) {
                uint8_t numb;
                read(fp, &numb, 1);
                sum += numb;
            }
            lseek(fp, 0, SEEK_SET);

            pthread_mutex_unlock(mutex);
        }
        printf("Thread %d complete in file %d sum = %llu\n", data->number, fileNumber, sum);

    }
    return NULL;
}

void readFromFile() {
    pthread_t *threads = (pthread_t *) malloc(I * sizeof(pthread_t));
    readFromFileData *threadData = (readFromFileData *) malloc(I * sizeof(readFromFileData));

    checkMalloc(threads, "Thread's Read")
    checkMalloc(threadData, "ThreadData Read")

    for (int i = 0; i < I; ++i) {
        threadData[i].number = i;
        threadData[i].fileNumber = i % COUNT_FILE;
    }

    for (int i = 0; i < I; ++i) {
        pthread_create(&(threads[i]), NULL, readFileThread, &threadData[i]);
    }
}
