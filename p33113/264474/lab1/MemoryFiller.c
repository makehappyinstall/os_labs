#include "MemoryFiller.h"
#include <time.h>

void *MemoryFillThread(void *rawParams) {

    struct MemoryFillerThreadParams *params = (struct MemoryFillerThreadParams *) rawParams;

    while (*params->infinityLoop) {

        for (size_t i = 0; i < params->numbersCount; ++i) {

            params->startAddress[i] = rand_r((unsigned) time(NULL));

        }

    }

    return NULL;
}
