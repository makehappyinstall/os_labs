#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

struct MemoryFillerThreadParams {
    int *startAddress;
    size_t numbersCount;
    int *infinityLoop;
};

void *MemoryFillThread(void *rawParams) {

    struct MemoryFillerThreadParams *params = (struct MemoryFillerThreadParams *) rawParams;

    while (*params->infinityLoop) {

        for (size_t i = 0; i < params->numbersCount; ++i) {

            params->startAddress[i] = rand();

        }

    }

    return NULL;
}
