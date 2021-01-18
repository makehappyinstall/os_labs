#define _GNU_SOURCE

#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <linux/futex.h>
#include <syscall.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>

#define A 81
#define B 0x8230E78E
#define D 73
#define E 168
#define I 137

typedef struct fillMemoryArgs {
    void *startAddress;
    size_t memorySize;
    FILE *urandom;
} fillArgs;

void *fillMemoryFromThread(void *args) {
    fillArgs *inArgs = (fillArgs *) args;
    int c = fread((void *) inArgs->startAddress, 1, inArgs->memorySize, inArgs->urandom);
    if(c <= 0){
        perror("Память не заполняется");
        _exit(1);
    }
    return 0;
}

void fillMemory(void *startAddress, size_t memorySize) {
    startAddress = mmap(startAddress, memorySize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if(startAddress == MAP_FAILED){
        perror("Не мапится");
        _exit(1);
    }
    FILE *urandom = fopen("/dev/urandom", "r");
    pthread_t threads[D];
    fillArgs args = {startAddress, memorySize, urandom};
    for(int i = 0; i < D; ++i)
        pthread_create(&threads[i], NULL, fillMemoryFromThread, (void *) &args);
    for(int i = 0; i < D; ++i)
        pthread_join(threads[i], NULL);
    fclose(urandom);
    munmap(startAddress, memorySize);
}

_Noreturn void *writeThread(void *fut) {
    int out = open("output", O_CREAT | O_WRONLY | O_TRUNC | O_DIRECT, 0700);
    if(out == -1){
        perror("Файл не открыть");
        _exit(1);
    }
    while (1) {
        FILE *urandom = fopen("/dev/urandom", "r");
        int isURandomEmpty = 0;
        while (!isURandomEmpty) {
            syscall(SYS_futex , (int *) fut, FUTEX_WAIT, 1, NULL, NULL, 0);
            *((int *) fut) = 1;
            int val;
            if (fread(&val, sizeof(int) - 1, 1, urandom) < 0) {
                isURandomEmpty = 1;
                printf("Рандом пуст");
            } else {
                val = abs(val);
                size_t inputSize = floor(log10(val) + 2);
                char str[inputSize];
                sprintf(str, "%d ", val);
                int c = write(out, str, inputSize);
                if(c <= 0){
                    perror("Нельзя записать в файл");
                    _exit(1);
                }
            }
            *((int *) fut) = 0;
            syscall(SYS_futex , (int *) fut, FUTEX_WAKE, 1, NULL, NULL, 0);
        }
        fclose(urandom);
    }
    close(out);
}

_Noreturn void *statisticsThread(void *fut) {
    while(1) {
        FILE *f = fopen("output", "r");
        if(f == NULL){
            continue;
        }
        syscall(SYS_futex , (int *) fut, FUTEX_WAIT, 1, NULL, NULL, 0);
        *((int *) fut) = 1;
        int num;
        long long sum = 0;
        while (fscanf(f, "%d ", &num) > 0)
            sum += (long long) num;
        printf("Сумма: %lld\n", sum);
        *((int *) fut) = 0;
        syscall(SYS_futex , (int *) fut, FUTEX_WAKE, 1, NULL, NULL, 0);
        fclose(f);
    }
}

void fillFile(long long fileSize) {
    int futTemp = 1;
    int *fut = &futTemp;
    pthread_t wThread;
    pthread_create(&wThread, NULL, writeThread, (void *) fut);
    *fut = 0;
    pthread_t statThreads[I];
    for (int i = 0; i < I; ++i)
        pthread_create(&statThreads[i], NULL, statisticsThread, (void *) fut);

    pthread_join(wThread, NULL);

    for (int i = 0; i < I; i++)
        pthread_join(statThreads[i], NULL);
}

int main() {
    fillMemory((void *) B, A * 1024 * 1024);
    fillFile(E * 1024 * 1024);
    return 0;
}
