#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>

#define MB_SIZE (1024 * 1024)
#define MEMORY_SIZE_MB 254
#define WRITE_THREADS 26
#define FILE_SIZE_MB 81
#define BLOCK_SIZE 135
#define READ_THREADS 70
#define FILES_NUMBER (MEMORY_SIZE_MB/FILE_SIZE_MB + (MEMORY_SIZE_MB % FILE_SIZE_MB == 0 ? 0 : 1))
#define CHUNK_SIZE ((MEMORY_SIZE_MB * MB_SIZE) / WRITE_THREADS)

#define SUCCESS 0

#define PBSTR "||||||||||||||||||||||||||||||||||||||||||||||||||"
#define PBWIDTH 50

int loopFlag = 1;
char *allocatedMemory;
char filenames[FILES_NUMBER][256];
sem_t sem;

struct threadArgs {
    pthread_t id;
    size_t threadIndex;
};

struct threadArgs writeThreadArgs[WRITE_THREADS];
struct threadArgs readThreadArgs[READ_THREADS];

void* readRandThread(void* tArgs);
void writeToMemory();
void writeSingleFile(char* fileName, void* start, int size);
void writeToFiles();
void* readFileThread(void* tArgs);
void readFromFiles();
void printProgress(double percentage);

int main(void) {
    char breakChar;

    for (int i = 0; i < FILES_NUMBER; i++)
        sprintf(filenames[i], "file%d.dat", i);

    printf("Before allocation [PRESS ENTER]\n");
    getchar();

    sem_init(&sem, 0, 1);
    allocatedMemory = malloc(MEMORY_SIZE_MB * MB_SIZE);
    puts("After allocation [PRESS ENTER]");
    getchar();

    printf("Filling memory with random numbers in %d threads...\n", WRITE_THREADS);
    writeToMemory();
    puts("After filling memory with data [PRESS ENTER]");
    getchar();

    puts("Writing data to files...");
    writeToFiles();

    free(allocatedMemory);
    puts("After deallocation [PRESS ENTER]");
    getchar();

    printf("Reading data from files in %d threads...\n", READ_THREADS);
    readFromFiles();

    puts("Press 'q' to break the loop");
    scanf("%c", &breakChar);
    if (breakChar == 'q')
        loopFlag = 0;

    while (loopFlag) {
        allocatedMemory = malloc(MEMORY_SIZE_MB * MB_SIZE);
        writeToMemory();
        printf("\nFilling memory with random numbers in %d threads...\n", WRITE_THREADS);
        writeToMemory();
        puts("Writing data to files...");
        writeToFiles();
        free(allocatedMemory);
        printf("Reading data from files in %d threads...\n", READ_THREADS);
        readFromFiles();

        puts("Press 'q' to break the loop");
        scanf("%c", &breakChar);
        if (breakChar == 'q')
            loopFlag = 0;
    }
    return 0;
}

void writeToMemory() {
    for (size_t i = 0; i < WRITE_THREADS; i++) {
        writeThreadArgs[i].threadIndex = i;
        int threadStatus = pthread_create(&writeThreadArgs[i].id, NULL, readRandThread, &writeThreadArgs[i]);
        if (threadStatus != 0)
            puts("Error occurred while writing to the memory");
    }
    for (size_t i = 0; i < WRITE_THREADS; i++)
        pthread_join(writeThreadArgs[i].id, NULL);
}

void* readRandThread(void* tArgs) {
    struct threadArgs *args = tArgs;
    sem_wait(&sem);
    int fd = open("/dev/urandom", O_RDONLY);
    int chunkSize = CHUNK_SIZE;
    if (args->threadIndex == WRITE_THREADS - 1)
        chunkSize += MEMORY_SIZE_MB * MB_SIZE - chunkSize * WRITE_THREADS;
    ssize_t result = read(fd, allocatedMemory + args->threadIndex * CHUNK_SIZE, chunkSize);
    close(fd);
    if (result == -1) {
        printf("Errno %d", errno);
        printf("failed to fill the area with size %d in thread %zu\n", chunkSize, args->threadIndex);
        return NULL;
    }
    sem_post(&sem);
    return SUCCESS;
}

void writeToFiles() {
    int fileSize = FILE_SIZE_MB * MB_SIZE;
    for (size_t i = 0; i < FILES_NUMBER; i++) {
        if (i == FILES_NUMBER - 1)
            fileSize = (MEMORY_SIZE_MB - FILE_SIZE_MB * (FILES_NUMBER - 1)) * MB_SIZE;
        printf("Writing to the file '%s':", filenames[i]);
        writeSingleFile(filenames[i], allocatedMemory + FILE_SIZE_MB * i, fileSize);
        printf("\nThe file '%s' was filled\n", filenames[i]);
    }
}

void writeSingleFile(char* fileName, void* start, int fileSize) {
    int blocksNumber = fileSize / BLOCK_SIZE + (fileSize % BLOCK_SIZE == 0 ? 0 : 1);
    int fd = open(fileName, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        printf("Failed opening the file '%s' for writing\n", fileName);
        exit(-1);
    }
    sem_wait(&sem);
    int blockSize = BLOCK_SIZE;
    for (size_t i = 0; i < blocksNumber; i++) {
        if (i == blocksNumber - 1)
            blockSize = fileSize - BLOCK_SIZE * (blocksNumber - 1);
        char* startPtr = (char*) start + i * BLOCK_SIZE;
        if (write(fd, startPtr, blockSize) == -1) {
            printf("\nFailed writing to the file '%s'\n", fileName);
            exit(-1);
        }
        printProgress((double) (i+1) / blocksNumber);
    }
    sem_post(&sem);
    close(fd);
}

void readFromFiles() {
    for (size_t i = 0; i < READ_THREADS; i++) {
        readThreadArgs[i].threadIndex = i;
        int threadStatus = pthread_create(&readThreadArgs[i].id, NULL, readFileThread, &readThreadArgs[i]);
        if (threadStatus != 0)
            puts("Error occurred while reading from the file");
    }
    for (size_t i = 0; i < READ_THREADS; i++) {
        pthread_join(readThreadArgs[i].id, NULL);
    }
}

void* readFileThread(void* tArgs) {
    struct threadArgs *args = tArgs;

    for (int i = 0; i < FILES_NUMBER; i++) {
        int fd = open(filenames[i], O_RDONLY);
        if (fd < 0) {
            printf("Failed opening the file '%s' for reading\n", filenames[i]);
            exit(-1);
        }

        sem_wait(&sem);
        off_t size = lseek(fd, 0L, SEEK_END);
        lseek(fd, 0, SEEK_SET);
        char *buffer = (char *) malloc(size);
        ssize_t readBytes = read(fd, buffer, size);

        long long sum = 0;
        for (size_t j = 0; j < readBytes / sizeof(char); j += 1) {
            sum += buffer[i];
        }
        if (args->threadIndex == 0) {
            printf("Sum in the file '%s': %lld\n", filenames[i], sum);
        }

        free(buffer);
        close(fd);
        sem_post(&sem);
    }
    return SUCCESS;
}

// prints progress bar
void printProgress(double percentage) {
    int val = (int) (percentage * 100);
    int lpad = (int) (percentage * PBWIDTH);
    int rpad = PBWIDTH - lpad;
    printf("\r%3d%% [%.*s%*s]", val, lpad, PBSTR, rpad, "");
    fflush(stdout);
}