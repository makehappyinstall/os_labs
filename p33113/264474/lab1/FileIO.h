#ifndef _FILE_H_
#define _FILE_H_
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <rpc.h>
#define blockSize    95//G
#define blockNumbersCount   blockSize / sizeof(int) //G
#define blockNumbersSize  blockNumbersCount *sizeof(int)

union IOBlock {
    char raw[blockSize];
    int numbers[blockNumbersCount];
};

struct FileWriterThreadParams {
    int file_id;
    int *startAddress;
    size_t blocksCount;
    int *infinityLoop;
    pthread_mutex_t *fileMutex;
};
void *FileWriterThread(void *rawParams);
struct FileReaderThreadParams {
    int filesCount;
    int *infinityLoop;
    long long int sum;
    struct FileWriterThreadParams *fileWriterThreadParams;
};
void *FileReaderThread(void *rawParams);
#endif