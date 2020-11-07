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

#define MB_TO_BYTE 1024*1024
#define A_BYTE A*MB_TO_BYTE // 274726912
#define E_BYTE E*MB_TO_BYTE

#define COUNT_BYTE_TO_WRITE A_BYTE/D
#define COUNT_FILE A/E

#define BLOCK_SIZE 4096
#define BLOCK_COUNT E_BYTE / BLOCK_SIZE

#define FILENAME "/tmp/file_%d.bin"

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

FILE **files;
FILE *pRandom;
uint8_t *memory;

genDataThread *genDataThreadData;
writeFileData *writeFileDataThreads;
readFromFileData *readFromFileDataThread;

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

void init() {
    for (int i = 0; i < BLOCK_COUNT; i++) {
        MBlockWrite[i].number = i;
        MBlockWrite[i].start = i * BLOCK_SIZE;
        MBlockWrite[i].end = (i + 1) * BLOCK_SIZE;

        MBlockRead[i].number = i;
        MBlockRead[i].start = i * BLOCK_SIZE;
        MBlockRead[i].end = (i + 1) * BLOCK_SIZE;
    }

    shuffle(MBlockRead, BLOCK_COUNT);

    files = (FILE**) malloc(COUNT_FILE * sizeof(FILE*));
    checkMalloc(files, "File arrays")

    char filename[35];
    for (int i = 0; i < COUNT_FILE; i++) {
        snprintf(filename, 35, FILENAME, i);
        files[i] = fopen(filename, "w+b");

        if(files[i] == NULL) {
            printf("Creat or write file '%s' is have error.\n", filename);
            exit(1);
        }

        pthread_cond_init(&(conds[i]), NULL);
        pthread_mutex_init(&(mutexs[i]), NULL);
    }

    writeFileDataThreads = (writeFileData *) malloc(COUNT_FILE * sizeof(writeFileData));
    checkMalloc(writeFileDataThreads, "ThreadData Write")
    for (int i = 0; i < COUNT_FILE; ++i) {
        writeFileDataThreads[i].number = i;
    }

    genDataThreadData = (genDataThread *) malloc(D * sizeof(genDataThread));
    checkMalloc(genDataThreadData, "ThreadData Generate")
    for (size_t i = 0; i < D - 1; i++) {
        genDataThreadData[i].start = i * COUNT_BYTE_TO_WRITE;
        genDataThreadData[i].stop = (i + 1) * COUNT_BYTE_TO_WRITE;
    }

    readFromFileDataThread = (readFromFileData *) malloc(I * sizeof(readFromFileData));
    checkMalloc(readFromFileDataThread, "ThreadData Read")
    for (int i = 0; i < I; ++i) {
        readFromFileDataThread[i].number = i;
        readFromFileDataThread[i].fileNumber = i % COUNT_FILE;
    }

    pRandom = fopen(RANDOM, "r");
}

int main() {
    int firstRun = 1;

    init();

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

    return 0;
}

void *threadRandomGenerate(void *thread_data) {
    genDataThread *data = (genDataThread *) thread_data;;

    for (size_t i = data->start; i < data->stop; i++) {
        uint8_t random = 0;
        fread(&random, sizeof(uint8_t), 1, pRandom);
        memory[i] = random;
    }

    return NULL;
}

void generateRandomData() {
    pthread_t *threads = (pthread_t *) malloc(D * sizeof(pthread_t));
    checkMalloc(threads, "Thread's Generate")

    for (int i = 0; i < D; i++)
        pthread_create(&(threads[i]), NULL, threadRandomGenerate, &genDataThreadData[i]);

    for (int i = 0; i < D; i++)
        pthread_join(threads[i], NULL);

    free(threads);
}

void *threadWriteToFile(void *thread_data) {
    writeFileData *data = (writeFileData *) thread_data;
    int thread = data->number;
    pthread_mutex_t *mutex = &(mutexs[thread]);
    pthread_cond_t *cond = &conds[thread];
    FILE *toWrite2 = files[thread];

    int offset = thread * E_BYTE;
    for (int j = 0; j < BLOCK_COUNT; j++) {
        size_t start = MBlockWrite[j].start;
        size_t end = MBlockWrite[j].end;

        pthread_mutex_lock(mutex);
        isWrite[thread] = 1;

        size_t curPos = ftell(toWrite2);
        fseek(toWrite2, start, SEEK_CUR);
        for (size_t k = start; k < end; k += G) {
            size_t offsetWriteArray = offset + k;
            fwrite((memory+offsetWriteArray), sizeof(uint8_t), G, toWrite2);
        }
        fseek(toWrite2, curPos, SEEK_SET);

        fflush(toWrite2);

        isWrite[thread] = 0;
        pthread_cond_signal(cond);
        pthread_mutex_unlock(mutex);

    }

    return NULL;
}

void writeToFile() {
    pthread_t *threads = (pthread_t *) malloc(COUNT_FILE * sizeof(pthread_t));
    checkMalloc(threads, "Thread's Write")

    for (int i = 0; i < COUNT_FILE; ++i) {
        pthread_create(&(threads[i]), NULL, threadWriteToFile, &writeFileDataThreads[i]);
    }

    for (int i = 0; i < COUNT_FILE; i++)
        pthread_join(threads[i], NULL);

    free(threads);
}

_Noreturn void *readFileThread(void *thread_data) {
    readFromFileData *data = (readFromFileData *) thread_data;

    int fileNumber = data->fileNumber;
    pthread_mutex_t *mutex = &(mutexs[fileNumber]);
    pthread_cond_t *cond = &(conds[fileNumber]);
    FILE *fp = files[fileNumber];

    while (1) {
        unsigned long long sum = 0;
        for (int i = 0; i < BLOCK_COUNT; i++) {
            size_t start = MBlockRead[i].start;
            size_t end = MBlockRead[i].end;

            pthread_mutex_lock(mutex);
            while (isWrite[fileNumber]) {
                pthread_cond_wait(cond, mutex);
            }

            size_t curPos = ftell(fp);
            fseek(fp, start, SEEK_SET);
            for (size_t j = start; j < end; j++) {
                uint8_t numb;
                fread(&numb, sizeof(uint8_t), 1, fp);
                sum += numb;
            }
            fseek(fp, curPos, SEEK_SET);

            pthread_mutex_unlock(mutex);
        }
        printf("Thread %d complete in file %d sum = %llu\n", data->number, fileNumber, sum);

    }

}

void readFromFile() {
    pthread_t *threads = (pthread_t *) malloc(I * sizeof(pthread_t));
    checkMalloc(threads, "Thread's Read")

    for (int i = 0; i < I; ++i) {
        pthread_create(&(threads[i]), NULL, readFileThread, &readFromFileDataThread[i]);
    }
}
