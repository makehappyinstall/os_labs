#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/mman.h>
#include "memory.h"

//A=45;B=0xF113ABAB;C=mmap;D=59;E=11;F=nocache;G=97;H=random;I=66;J=max;K=sem

#define A_MEM_SIZE 45*1024*1024
#define B_FIRST_ADDRESS 0xF113ABAB
#define D_THREADS_NUM_FOR_MEM 59
#define I_THREADS_FOR_FILE 66
#define E_FILE_SIZE 11*1024*1024
#define G_BLOCK_SIZE 97

#define FILES_NUMBER ((A_MEM_SIZE) / (E_FILE_SIZE))

int * memRegion;

typedef struct{
    int thrNum;
    char * memSegment;
    int size;
    int fd;
} memFillThreadData;

typedef struct{
    int fileNum;
    int thrNum;
} fileReadThreadData;

int stopInfCycle = 0;
int maxValue  = INT_MIN;
sem_t semaphoreFile[FILES_NUMBER+1];

int* allocate(){
	return mmap((void*) B_FIRST_ADDRESS, A_MEM_SIZE,
			PROT_EXEC | PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS,
			-1,
			0);
}

void writeRandomBlock(int fd, int fileSize, int blocksNum, char * buffer, char * vMem) {
    int blockNumber;
    int startByte;
    int blockSize;

    blockNumber = rand() % (blocksNum + 1);
    startByte   = blockNumber * G_BLOCK_SIZE;
    blockSize   = blockNumber == blocksNum ? fileSize - (G_BLOCK_SIZE*blocksNum) : G_BLOCK_SIZE;

    memcpy(buffer, vMem + startByte, blockSize);
    pwrite(fd, buffer, blockSize, startByte);
}

void readRandomBlock(int fd, int fileSize, int blocksNum, char * buffer, char * vMem) {
    int blockNumber;
    int startByte;
    int blockSize;

    blockNumber = rand() % (blocksNum + 1);
    startByte   = blockNumber * G_BLOCK_SIZE;
    blockSize   = blockNumber == blocksNum ? fileSize - (G_BLOCK_SIZE*blocksNum) : G_BLOCK_SIZE;

    pread(fd, buffer, blockSize, startByte);
    memcpy(vMem + startByte, buffer, blockSize);
}

void writeFile(int start, int fd, int fileSize){
    int blocksNumber;
    char * buffer;
    char * vMemory;

    blocksNumber = fileSize / G_BLOCK_SIZE;
    buffer       =  (char *) malloc(G_BLOCK_SIZE);
    vMemory      =  (char *) memRegion + start;

    for(int i = 0; i <= blocksNumber * 2; i++){
        writeRandomBlock(fd, fileSize, blocksNumber, buffer, vMemory);
    }
    free(buffer);
}



void readFile(int* memRegion, int fd, int fileSize){
    int blocksNumber;
    char * buffer;
    char * vMemory;

    blocksNumber = fileSize / G_BLOCK_SIZE;
    buffer       = (char *) malloc(G_BLOCK_SIZE);
    vMemory      = (char *) memRegion;

    for(int i = 0; i <= blocksNumber * 2; i++){
        readRandomBlock(fd, fileSize, blocksNumber, buffer, vMemory);
    }
    free(buffer);
}

void* fillMemoryThreadMethod(void * params_void){
    memFillThreadData * threadData;

    threadData = (memFillThreadData *) params_void;

    printf("Generator_%i начал работу\n", threadData->thrNum);

    do pread(threadData->fd, threadData->memSegment, threadData->size, 0);
    while (!stopInfCycle);

    printf("Generator_%i закончил работу\n", threadData->thrNum);
    return NULL;
}


void* writeToFiles(int isFirstRun) {
    char filename[16];
    int fd;

    for(int i=0; i<FILES_NUMBER; i++){
        sprintf(filename, "lab1_%i.bin", i);

        if(!isFirstRun) sem_wait(&semaphoreFile[i]);

        
        fd = open(filename,O_CREAT | O_WRONLY | O_DIRECT | O_TRUNC, __S_IREAD | __S_IWRITE);
        writeFile(E_FILE_SIZE/4*i, fd, E_FILE_SIZE);

        close(fd);
        sem_post(&semaphoreFile[i]);
    }
    sprintf(filename, "lab1_%i.bin", FILES_NUMBER);
    fd = open(filename, O_CREAT | O_WRONLY | O_DIRECT | O_TRUNC, __S_IREAD | __S_IWRITE);

    writeFile(E_FILE_SIZE/4*FILES_NUMBER, fd, A_MEM_SIZE - (FILES_NUMBER * E_FILE_SIZE));

    close(fd);

    sem_post(&semaphoreFile[FILES_NUMBER]);

    return NULL;
}

void* writeThreadMethod(void * params_void){
    int isFirstRun;

    printf("Writer начал работу\n");

    isFirstRun = 1;

    do{
        writeToFiles(isFirstRun);
        if(isFirstRun) isFirstRun = 0;
    } while (!stopInfCycle);

    printf("Writer закончил работу\n");
    return NULL;
}

void findMax(int * memory, int fileSize) {
    for(int i=0; i<fileSize/4; i++){
        if(memory[i]>maxValue){
            maxValue = memory[i];
        }
    }
}

void* readThreadMethod(void * params_void){
    fileReadThreadData * threadData = (fileReadThreadData *) params_void;
    printf("Reader_%i работает с файлом №%i\n", threadData->thrNum, threadData->fileNum);

    do{
        char filename[16];
        int fileSize, fd;
        int * memory;

        sprintf(filename, "lab1_%i.bin", threadData->fileNum);
        sem_wait(&semaphoreFile[threadData->fileNum]);
        fd = open(filename, O_RDONLY);
        if(fd == -1){
            sem_post(&semaphoreFile[threadData->fileNum]);
            printf("Reader_%i не может открыть файл %i\n", threadData->thrNum, threadData->fileNum);
            return NULL;
        }
        if (threadData->fileNum == FILES_NUMBER) fileSize = A_MEM_SIZE - (FILES_NUMBER * E_FILE_SIZE);
        else fileSize = E_FILE_SIZE;

        memory = (int *) malloc(fileSize);
        readFile(memory, fd, fileSize);
        close(fd);
        sem_post(&semaphoreFile[threadData->fileNum]);

        findMax(memory, fileSize);

        free(memory);
    } while (!stopInfCycle);

    printf("Reader_%i закончил работу\n", threadData->thrNum);
    return NULL;
}

void blockWhileFilesNotCreated() {
    for(int i=0; i<=FILES_NUMBER; i++){
        sem_init(&semaphoreFile[i], 0, 1);
        sem_wait(&semaphoreFile[i]);
    }
}

void destroySems() {
    for(int i=0; i<=FILES_NUMBER; i++){
        sem_destroy(&semaphoreFile[i]);
    }
}

void createFileReadersThreads(fileReadThreadData * readerData, pthread_t * readers) {
    for(int i=0; i<I_THREADS_FOR_FILE; i++){
        readerData[i].thrNum=i;
        readerData[i].fileNum = i % FILES_NUMBER;
        pthread_create(&(readers[i]), NULL, readThreadMethod, &(readerData[i]));
    }
}

void createFileWriterThread(pthread_t * writer) {
    pthread_create(writer, NULL, writeThreadMethod, NULL);
}

int main() {
    int fd, segSize;
    int startNum, endNum;
    pthread_t * fillers;
    memFillThreadData * fillersData;

    pthread_t * writer;
    pthread_t * readers;

    fileReadThreadData * readerData;

    printf("До аллокации памяти. Нажмите любую кнопку, чтобы продолжить.\n");
    getchar();
    memRegion = allocate();
    if (memRegion == MAP_FAILED) {
        printf("Mapping Failed\n");
        return -1;
    }

    printf("После аллокации памяти. Нажмите любую кнопку, чтобы продолжить.\n");
    getchar();

    stopInfCycle = 0;

    fd              = open("/dev/urandom", O_RDONLY);
    fillers         = (pthread_t*) malloc(D_THREADS_NUM_FOR_MEM * sizeof(pthread_t));
    fillersData     = (memFillThreadData *) malloc(D_THREADS_NUM_FOR_MEM * sizeof(memFillThreadData));
    segSize         = (A_MEM_SIZE / 4 / D_THREADS_NUM_FOR_MEM) + 1;


    for(int i=0; i<D_THREADS_NUM_FOR_MEM-1; i++){
        startNum  = i * segSize;
        endNum    = (i + 1) * segSize;

        fillersData[i].thrNum    = i;
        fillersData[i].memSegment = (char *)(memRegion + startNum * 4);
        fillersData[i].size = (endNum - startNum) * 4;
        fillersData[i].fd = fd;
        pthread_create(&(fillers[i]), NULL, fillMemoryThreadMethod, &fillersData[i]);
    }
    startNum  = (D_THREADS_NUM_FOR_MEM-1) * segSize;
    endNum    = A_MEM_SIZE / 4;
    fillersData[D_THREADS_NUM_FOR_MEM-1].thrNum    = D_THREADS_NUM_FOR_MEM-1;
    fillersData[D_THREADS_NUM_FOR_MEM-1].memSegment = (char *)(memRegion + startNum * 4);
    fillersData[D_THREADS_NUM_FOR_MEM-1].size = (endNum - startNum) * 4;
    fillersData[D_THREADS_NUM_FOR_MEM-1].fd = fd;

    pthread_create(&(fillers[D_THREADS_NUM_FOR_MEM-1]), NULL, fillMemoryThreadMethod, &fillersData[D_THREADS_NUM_FOR_MEM-1]);

    blockWhileFilesNotCreated();

    writer = (pthread_t *) malloc(sizeof(pthread_t));
    createFileWriterThread(writer);

    readerData = (fileReadThreadData *) malloc(I_THREADS_FOR_FILE*sizeof(fileReadThreadData));
    readers   = (pthread_t*) malloc(I_THREADS_FOR_FILE * sizeof(pthread_t));
    createFileReadersThreads(readerData, readers);

    printf("Нажмите любую кнопку, чтобы прервать бесконечный цикл.\n");
    getchar();
    stopInfCycle=1;

    pthread_join(*writer, NULL);
    for(int i=0; i<D_THREADS_NUM_FOR_MEM; i++){
        pthread_join(fillers[i], NULL);
    }
    for(int i=0; i<I_THREADS_FOR_FILE; i++){
        pthread_join(readers[i], NULL);
    }

    printf("После заполнения памяти. Нажмите любую кнопку, чтобы продолжить.\n");
    getchar();
    printf("Максимум:  %i\n", maxValue);
    printf("Освобождение памяти...\n");


    free(fillers);
    free(fillersData);
    free(writer);
    free(readerData);
    free(readers);

    destroySems();
    close(fd);

    if (munmap(memRegion, A_MEM_SIZE) == -1) {
        printf("Mummap failed\n");
        return -1;
    }
    return 0;
}
