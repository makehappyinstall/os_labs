#include <stdio.h>
#include <memoryapi.h>
#include <pthread.h>
#include <math.h>
#include <time.h>
#include "MemoryFiller.h"
#include "FileIO.h"
//A=31;
//B=0xC9E9A85E;
//C=mmap;
//D=37;
//E=165;
//F=block;
//G=95;
//H=random;
//I=16;
//J=sum;
//K=futex;
int isLooping = 1;

int main() {

//    srand((unsigned) time(NULL));

    int A = 31; // область памяти мб
//    void *B = (void *) 0x00000001; // адрес начала области памяти
    void *B = (void *) 0xC9E9A85E; // адрес начала области памяти
    // C=mmap; способ выделения памяти
    size_t D = 37; // кол-во потоков на запись в память
    int E = 165; // Файлы размера мб
    // F=block; обращение к диску
    int G = 95; // размер блока вв байт
    // H=random; последовательный способ записи\чтения блоков
    size_t I = 16; //потоках осуществлять чтение данных из файлов
    // J=sum; фгрегирующая функция
    // K=futex; примитив синхронизации при вв фйлов


#pragma region Выделяем память

    printf("VirtualAlloc start");
    getchar();

    // выделяем память размером A c адреса B
    int *start = (int *) VirtualAlloc(B, A * 1024 * 1024, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (start == NULL) {
        printf("Cannot alloc at the address ");
        start = (int *) VirtualAlloc(NULL, A * 1024 * 1024, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    }

    printf("VirtualAlloc end");
    getchar();

#pragma endregion Выделяем память

#pragma region Заполняем память числами

    void *startAddress = start; // количество чисел, которые мы сможем записать
    size_t numbersCount = A * 1024 * 1024 / sizeof(int); // количество чисел, которые мы сможем записать
    size_t numbersOnThread = numbersCount / D;  // количество чисел на поток
    size_t addressesOnThread = numbersOnThread / sizeof(int);  // количество чисел на поток

    //    pthread_t *fillerThreads = (pthread_t *) VirtualAlloc(NULL,D * sizeof(pthread_t),MEM_COMMIT | MEM_RESERVE,PAGE_READWRITE);
    //    pthread_t *writer_thread = (pthread_t *) VirtualAlloc(NULL,sizeof(pthread_t),MEM_COMMIT | MEM_RESERVE,PAGE_READWRITE);
    //    pthread_t *reader_threads = (pthread_t *) VirtualAlloc(NULL,I * sizeof(pthread_t),MEM_COMMIT | MEM_RESERVE,PAGE_READWRITE);

    pthread_t fillerThreads[D];

    struct MemoryFillerThreadParams fillerThreadParams[D];

    for (size_t i = 0; i < D - 1; ++i, startAddress += addressesOnThread) {
        fillerThreadParams[i].startAddress = startAddress;
        fillerThreadParams[i].numbersCount = numbersOnThread;
        fillerThreadParams[i].infinityLoop = &isLooping;
        pthread_create(&fillerThreads[i], NULL, MemoryFillThread, &fillerThreadParams[i]);
    }

    // Остаток закидываем в последний поток
    size_t restNumbersCount = numbersCount - D * numbersOnThread;

    fillerThreadParams[D - 1].startAddress = startAddress;
    fillerThreadParams[D - 1].numbersCount = numbersOnThread + restNumbersCount;
    fillerThreadParams[D - 1].infinityLoop = &isLooping;
    pthread_create(&fillerThreads[D - 1], NULL, MemoryFillThread, &fillerThreadParams[D - 1]);


#pragma endregion Заполняем память числами

#pragma region пишем в файлы

    size_t filesCount = A / E;
    size_t blocksPerFile = E * 1024 * 1024 / blockSize;
    size_t totalBlocks = A * 1024 * 1024 / (blockNumbersSize);
    size_t restBlocks = totalBlocks - filesCount * blocksPerFile;
    filesCount++;


    printf("%d blocksPerFile, %d restBlocks", blocksPerFile, restBlocks);
    pthread_t writerThreads[filesCount];
    pthread_mutex_t FileMutexes[filesCount];

    struct FileWriterThreadParams writerThreadParams[filesCount];


    void * WriteStartAddress = start;
    for (size_t i = 0;
         i < filesCount - 1; ++i, WriteStartAddress += blocksPerFile * blockNumbersSize) {
        pthread_mutex_init(&FileMutexes[i], NULL);
        writerThreadParams[i].infinityLoop = &isLooping;
        writerThreadParams[i].blocksCount = blocksPerFile;
        writerThreadParams[i].startAddress = WriteStartAddress;
        writerThreadParams[i].fileMutex = &FileMutexes[i];
        writerThreadParams[i].file_id = i;


        pthread_create(&writerThreads[i], NULL, FileWriterThread, &writerThreadParams[i]);
    }

    pthread_mutex_init(&FileMutexes[filesCount - 1], NULL);
    writerThreadParams[filesCount - 1].infinityLoop = &isLooping;
    writerThreadParams[filesCount - 1].blocksCount =restBlocks;
    writerThreadParams[filesCount - 1].startAddress = startAddress;
    writerThreadParams[filesCount - 1].fileMutex = &FileMutexes[filesCount - 1];
    writerThreadParams[filesCount - 1].file_id = filesCount - 1;

    pthread_create(&writerThreads[filesCount - 1], NULL, FileWriterThread, &writerThreadParams[filesCount - 1]);

#pragma  endregion пишем в файлы

#pragma region читаем файлы

    pthread_t readerThreads[I];
    struct FileReaderThreadParams readerThreadsParams[I];

    for (size_t i = 0; i <I; ++i) {

        readerThreadsParams[i].sum = 0;
        readerThreadsParams[i].infinityLoop = &isLooping;
        readerThreadsParams[i].filesCount = filesCount;
        readerThreadsParams[i].fileWriterThreadParams = &writerThreadParams;


        pthread_create(&readerThreads[i], NULL, FileReaderThread, &readerThreadsParams[i]);
    }


#pragma  endregion читаем файлы

    printf("\ninfinity Loop");
    getchar();
    printf("stopping");
    isLooping = 0;
    //ожидаем завершения всех потоков генерации, Null - результат возвращаемый потоком
    for (int i = 0; i < D; i++)
        pthread_join(fillerThreads[i], NULL);

    for (int i = 0; i < filesCount; i++) {
        pthread_join(writerThreads[i], NULL);
    }

    for (int i = 0; i < I; i++) {
        pthread_join(readerThreads[i], NULL);
    }

    for (size_t i = 0; i < filesCount - 1; ++i) {
        pthread_mutex_destroy(&FileMutexes[i]);
    }

    printf("\nBefore deallocation");
    getchar();

    VirtualFree(start, 0, MEM_RELEASE);

    printf("After deallocated");
//    printf("sum %d", sum);
    getchar();
    return 0;
}
