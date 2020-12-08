#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "CpuTime.h"

#define A 244
// C malloc
#define D 28
#define E 193
// F nocashe
#define G 147
// H random
#define I 149
// J sum
// K futex
#define megabyte_size 1024*1024
#define FILES_NUMBER (A/E + (A % E == 0 ? 0 : 1))
#define ERROR_CREATE_THREAD -11
#define ERROR_JOIN_THREAD   -12
#define SUCCESS               0

void *mem_pointer;
int randomNumb;
double IOCPUTime = 0;
pthread_mutex_t mutex;

void *use_random(void *vargPtr) {
    int threadIndex = (intptr_t) vargPtr;
    int blockSize = A / D;
    void *ptrStart = mem_pointer + threadIndex * blockSize;
    if (threadIndex == D - 1) blockSize += A - (A / D) * D;
    if (read(randomNumb, ptrStart, blockSize) == -1) {
        printf("Не удалось заполнить область с %p, размер = %d\n", ptrStart, blockSize);
        return NULL;
    }
    return SUCCESS;
}

void fill_memory() {
    int status; //для создания потоков
    int status_addr;

    printf("Заполняем случайными числами в %d потоков\n", D);
    randomNumb = open("/dev/urandom", O_RDONLY);
    if (randomNumb < 0) {
        printf("Не удалось открыть %s\n", "/dev/urandom");
        exit(1);
    }

    pthread_t threads[D];

    for (int i = 0; i < D; i++) {
        status = pthread_create(&threads[i], 0, use_random, (void *) (intptr_t) i);
        if (status != 0) {
            printf("Не удалось создать поток, статус = %d\n", status);
            exit(ERROR_CREATE_THREAD);
        }
    }

    for (int i = 0; i < D; i++) {
        status = pthread_join(threads[i], (void **) &status_addr);
        if (status_addr != SUCCESS) {
            printf("Не удалось выполнить поток, статус = %d\n", status);
            exit(ERROR_JOIN_THREAD);
        }
    }
    close(randomNumb);
    printf("Замеряем после заполнения участка данными (Enter - далее)\n");
    getchar();
}

void write_to_single_file(char *fileName, void *start, int size) {
    double startIOCPUTime, endIOCPUTime;

    int fd = open(fileName, O_CREAT | O_WRONLY, S_IRWXU);
    if (fd < 0) {
        printf("Не удалось открыть файл '%s' для записи\n", fileName);
        exit(-1);
    }

    int blocksNumber = size / G;
    if (G * blocksNumber < size)
        blocksNumber++;

    startIOCPUTime = getCPUTime();
    pthread_mutex_lock(&mutex);

    for (int i = 0; i < blocksNumber; i++) {
        ssize_t wroteBytes;
        if(i == blocksNumber - 1)
            wroteBytes = write(fd, start + i * G, size - G * (blocksNumber - 1));
        else
            wroteBytes = write(fd, start + i * G, G);

        if (wroteBytes == -1) {
            printf("\nНе удалось записать в файл '%s'\n", fileName);
            exit(-1);
        }
    }

    pthread_mutex_unlock(&mutex);
    close(fd);
    endIOCPUTime = getCPUTime();
    IOCPUTime += endIOCPUTime - startIOCPUTime;
    printf("Файл '%s' записан!\n", fileName);
}

void write_to_file() {
    char fileName[9];
    printf("Записываем данные из памяти в файлы...\n");
    for (int i = 0; i < FILES_NUMBER; i++) {

        sprintf(fileName, "output_%d", i);

        if (i == FILES_NUMBER - 1)
            write_to_single_file(fileName, mem_pointer + megabyte_size * E * i, (A - E * (FILES_NUMBER - 1)) * megabyte_size);
        else
            write_to_single_file(fileName, mem_pointer + megabyte_size * E * i, E * megabyte_size);
    }
}

void *print_file_sum() {
    double startIOCPUTime, endIOCPUTime;
    char fileName[9];
    for (int i = 0; i < FILES_NUMBER; i++) {
        sprintf(fileName, "output_%d", i);
        int fd = open(fileName, O_RDONLY);
        startIOCPUTime = getCPUTime();
        if (fd < 0) {
            printf("Не удалось открыть файл '%s' для чтения\n", fileName);
            exit(-1);
        }
        pthread_mutex_lock(&mutex);
        off_t size = lseek(fd, 0L, SEEK_END);
        lseek(fd, 0, SEEK_SET);
        __uint8_t *data = (__uint8_t *) malloc(size);
        ssize_t readBytes = read(fd, data, size);
        close(fd);
        pthread_mutex_unlock(&mutex);
        endIOCPUTime = getCPUTime();
        IOCPUTime += endIOCPUTime - startIOCPUTime;
        __int64_t sum = 0;
        for (size_t i = 0; i < readBytes / sizeof(__int8_t); i ++)
            sum += data[i];

        printf("Сумма агрегированных данных в файле '%s' = %ld\n", fileName, sum);
        free(data);
    }
}

void read_from_file() {
    int status_create; //для создания потоков
    int status_join;

    printf("Подсчитываем характеристики данных %d потоков...\n", I);
    pthread_t threads[I];
    for (int i = 0; i < I; i++) {
        printf("Выполняем поток %d \n", i + 1);
        status_create = pthread_create(&threads[i], NULL, print_file_sum, NULL);
        if (status_create != 0) {
            printf("Не удалось создать поток, статус = %d\n", status_create);
            exit(ERROR_CREATE_THREAD);
        }

        status_join = pthread_join(threads[i], NULL);
        if (status_join != 0) {
            printf("Не удалось выполнить поток, статус = %d\n", status_join);
            exit(ERROR_JOIN_THREAD);
        }
    }
}

void free_memory() {
    free(mem_pointer);
    printf("Замеряем после деаллокации (Enter - далее)\n");
    getchar();
}

int main() {
    double startTime, endTime;
    startTime = getCPUTime();
    char flag = '1';
    printf("Замеряем до аллокации (Enter далее)\n");
    getchar();
    pthread_mutex_init(&mutex, NULL);
    while (flag == '1') {
        mem_pointer = malloc(A * megabyte_size);
        printf("Замеряем после аллокации (Enter - далее)\n");
        getchar();
        fill_memory();
        write_to_file();
        free_memory();
        read_from_file();
        puts("Остановить беск. цикл? 0 - yes");
        scanf("%c", &flag);
    }
    pthread_mutex_destroy(&mutex);
    endTime = getCPUTime();
    printf("Затраченное на выполнение программы процесс. время = %lf\n", (endTime - startTime));
    printf("Затраченное на выполнение ввода/вывода процесс. время = %lf\n", IOCPUTime);
    return 0;
}
