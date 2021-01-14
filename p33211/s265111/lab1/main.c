#include <pthread.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>

//A=276;B=0x33F2678F;C=mmap;D=77;E=37;F=block;G=120;H=random;I=91;J=max;K=cv
#define ALLOCATED_MEMORY_SIZE_MB 276
#define START_ADDRESS 0x33F2678F
#define WRITE_IN_MEMORY_THREADS_CNT 77
#define FILES_SIZE_MB 37
#define IO_BUFFER_SIZE_B 120
#define READ_FILES_THREADS_CNT 91

typedef struct {
    FILE* src;
    int length;
    char* start;
} WriteInMemoryProps;

typedef struct {
    char* src;
    char* fileName;
    pthread_mutex_t* mutex;
    pthread_cond_t* cv;
} WriteInFileProps;

typedef struct {
    char* fileName;
    pthread_mutex_t* mutex;
    pthread_cond_t* cv;
} ReadFileProps;

void* writeInMemory(void* props) {
    WriteInMemoryProps* data = props;
    char* start = data->start;
    int length = data->length;
    FILE* src = data->src;
    size_t i = 0;
    while (i < length) {
        i += fread(start + i, 1, length - i, src);
    }
    return NULL;
}

//writing data from /dev/urandom in memory beginning from start address
void writeInMemoryFromUrandom(char* startAddress) {
    FILE* urandom = fopen("/dev/urandom", "r");

    int writePart = ALLOCATED_MEMORY_SIZE_MB * 1024 * 1024 / WRITE_IN_MEMORY_THREADS_CNT;

    pthread_t writeInMemoryThreads[WRITE_IN_MEMORY_THREADS_CNT];

    for (int i = 0; i < WRITE_IN_MEMORY_THREADS_CNT - 1; i++) {
        WriteInMemoryProps* props = malloc(sizeof(WriteInMemoryProps));
        props->start = startAddress;
        props->length = writePart;
        props->src = urandom;
        pthread_create(&(writeInMemoryThreads[i]), NULL, writeInMemory, props);
        startAddress += writePart;
    }

    WriteInMemoryProps* lastThreadProps = malloc(sizeof(WriteInMemoryProps));
    lastThreadProps->start = startAddress;
    lastThreadProps->length = writePart + ALLOCATED_MEMORY_SIZE_MB * 1024 * 1024 % WRITE_IN_MEMORY_THREADS_CNT;
    lastThreadProps->src = urandom;
    pthread_create(&(writeInMemoryThreads[WRITE_IN_MEMORY_THREADS_CNT - 1]), NULL, writeInMemory, lastThreadProps);

    for (int i = 0; i < WRITE_IN_MEMORY_THREADS_CNT; i++)
        pthread_join(writeInMemoryThreads[i], NULL);
    fclose(urandom);
}

_Noreturn void* infiniteGenerating(void* startAddress) {
    while (1) {
        writeInMemoryFromUrandom(startAddress);
    }
}

double generateRandom() {
    return (double)rand() / (double)RAND_MAX ;
}

void writeInFile(char* src, char* fileName, pthread_mutex_t* mutex, pthread_cond_t* cv) {
    pthread_mutex_lock(mutex);
    FILE* file = fopen(fileName, "wb");
    int file_size = FILES_SIZE_MB * 1024 * 1024;
    for (int j = 0; j < file_size / IO_BUFFER_SIZE_B; j++) {
        int blockNumber = (int) (generateRandom() * file_size / IO_BUFFER_SIZE_B);
        int blockSize = IO_BUFFER_SIZE_B;
        if (blockNumber == (file_size / IO_BUFFER_SIZE_B) && file_size % IO_BUFFER_SIZE_B != 0) {
            //this is a last block, and we should adjust block size
            blockSize = file_size % IO_BUFFER_SIZE_B;
        }
        fseek(file, blockNumber * IO_BUFFER_SIZE_B, SEEK_SET);
        fwrite(src, 1, blockSize, file);
    }
    fclose(file);
    printf("Data generated for %s\n", fileName);
    pthread_cond_broadcast(cv);
    pthread_mutex_unlock(mutex);
}

_Noreturn void* writeFromMemoryToFile(void* props) {
    WriteInFileProps* args = props;
    while (1) {
        writeInFile(args->src, args->fileName, args->mutex, args->cv);
    }
}

int find_max(int* begin, int length) {
    int max = INT_MIN;
    for (int* i = begin; i < begin + length; i++) {
        int val = *i;
        if (val > max)
            max = val;
    }
    return max;
}

_Noreturn void* readFile(void* props) {
    ReadFileProps* args = props;
    char* fileName = args->fileName;
    while (1) {
        int max = INT_MIN;
        pthread_mutex_lock(args->mutex);
        printf("Waiting on cv %s \n", fileName);
        pthread_cond_wait(args->cv, args->mutex);
        printf("Waiting complete for cv on %s \n", fileName);
        FILE* file = fopen(fileName, "rb");
        unsigned char buf[IO_BUFFER_SIZE_B];
        int file_size = FILES_SIZE_MB * 1024 * 1024;
        for (int i = 0; i < file_size / IO_BUFFER_SIZE_B; i++) {
            int blockNumber = (int) (generateRandom() * file_size / IO_BUFFER_SIZE_B);
            int blockSize = IO_BUFFER_SIZE_B;
            if (blockNumber == (file_size / IO_BUFFER_SIZE_B) && file_size % IO_BUFFER_SIZE_B != 0) {
                //this is a last block, and we should adjust block size
                blockSize = file_size % IO_BUFFER_SIZE_B;
            }
            fseek(file, blockNumber * IO_BUFFER_SIZE_B, SEEK_SET);
            if (fread(&buf, 1, blockSize, file) != blockSize) {
                continue;
            }
            int localMax = find_max((int* ) buf, IO_BUFFER_SIZE_B / 4);
            if (localMax > max)
                max = localMax;
        }
        printf("Max in %s: %d\n", fileName, max);
        fclose(file);
        pthread_mutex_unlock(args->mutex);
    }
}

char* allocate_memory() {
    return mmap((void* ) START_ADDRESS, ALLOCATED_MEMORY_SIZE_MB * 1024 * 1024, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

void allocateDeallocateMemory() {
    printf("Before memory allocation.");
    getchar();
    char* allocatedStartAddress = allocate_memory();
    printf("After memory allocation.");
    getchar();
    writeInMemoryFromUrandom(allocatedStartAddress);
    printf("After memory filling.");
    getchar();
    munmap(allocatedStartAddress, ALLOCATED_MEMORY_SIZE_MB * 1024 * 1024);
    printf("After memory deallocation.");
    getchar();
}

char* getFileNameByNumber(int fileNumber) {
    char* fileName = malloc(sizeof(char) * 11);
    snprintf(fileName, 11, "file_%d.bin", fileNumber);
    return fileName;
}

int main() {
    allocateDeallocateMemory();

    char* allocatedStartAddress = allocate_memory();

    const unsigned int filesCnt = ALLOCATED_MEMORY_SIZE_MB / FILES_SIZE_MB + 1;
    pthread_mutex_t mutexes[filesCnt];
    pthread_cond_t cvs[filesCnt];
    for (int i = 0; i < filesCnt; i++) {
        pthread_mutex_init(&(mutexes[i]), NULL);
        pthread_cond_init(&(cvs[i]), NULL);
    }

    pthread_t readThreads[READ_FILES_THREADS_CNT];
    int fileNumber = 0;
    for (int i = 0; i < READ_FILES_THREADS_CNT; i++) {
        if (fileNumber == filesCnt)
            fileNumber = 0;
        ReadFileProps* props = malloc(sizeof(ReadFileProps));
        props->fileName = getFileNameByNumber(fileNumber);
        props->mutex = &(mutexes[fileNumber]);
        props->cv = &(cvs[fileNumber]);
        pthread_create(&readThreads[i], NULL, readFile, props);
        fileNumber++;
    }

    pthread_t writeInMemory;
    pthread_create(&writeInMemory, NULL, infiniteGenerating, allocatedStartAddress);

    pthread_t writeThreads[filesCnt];
    for (int i = 0; i < filesCnt; i++) {
        WriteInFileProps* props = malloc(sizeof(WriteInFileProps));
        props->src = allocatedStartAddress;
        props->fileName = getFileNameByNumber(i);
        props->mutex = &(mutexes[i]);
        props->cv = &(cvs[i]);
        pthread_create(&writeThreads[i], NULL, writeFromMemoryToFile, props);
    }

    for (int i = 0; i < READ_FILES_THREADS_CNT; i++)
        pthread_join(readThreads[i], NULL);
    pthread_join(writeInMemory, NULL);
    for (int i = 0; i < filesCnt; i++) {
        pthread_join(writeThreads[i], NULL);
    }

    return 0;
}
