#ifndef _MEM_H_
#define _MEM_H_
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
struct MemoryFillerThreadParams {
    int *startAddress;
    size_t numbersCount;
    int *infinityLoop;
};
void *MemoryFillThread(void *rawParams);
#endif
