#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <semaphore.h>

// в байтах
#define THREADS_COUNT 29 //D
#define FILE_SIZE 91*1000*1000 //E
#define MAX_INT 2147483647
#define BLOCK_SIZE 33 //G
#define MEMORY_SIZE 190*1000*1000 //A
#define READ_THREADS_COUNT 17 //I
//в последнем файле будет меньше данных
#define COUNT_OF_FILES ((MEMORY_SIZE/1000000)/(FILE_SIZE/1000000) + (MEMORY_SIZE % FILE_SIZE != 0 ? 1 : 0))

//структура для данных потока
//typedef struct {
//    // указатель на
//    int *array;
//    int number;
//} pthrData;

void start_reading_random_threads();

void *threadFunc(void *i);

void start_write();

void analyze_files();

void *threadReadingFunc(void *number);

void writeToFileByBlock(char* p_start, int file, int size);
// семафор
sem_t semaphore;
void *array;

int main() {
    printf("Start program\n");
    array = malloc(MEMORY_SIZE);
    printf("Launch threads, which reads dev/random\n");
    start_reading_random_threads();

    printf("ARRAY\n");
    int* arrayOfInt = (int*) array;
    for (int i = 0; i < 100; i++)
        printf("%d ", arrayOfInt[i]);

    printf("Start writing to files\n");
    start_write();
    printf("Start analyzing files\n");
    analyze_files();
    free(array);
    printf("End program");
    return 0;
}

void analyze_files() {
    // семафор для потоков
    sem_init(&semaphore, 0, 2);

    //выделяем память под массив потоков
    pthread_t *p_threads = (pthread_t *) malloc(READ_THREADS_COUNT * sizeof(pthread_t));

    //запускаем потоки
    for (int i = 0; i < READ_THREADS_COUNT; i++) {
        pthread_create(&(p_threads[i]), NULL, threadReadingFunc, (void *) i);
    }

    //ожидаем выполнение потоков
    for (int i = 0; i < READ_THREADS_COUNT; i++)
        pthread_join(p_threads[i], NULL);

    sem_destroy(&semaphore);
}

// записываем array -> files
void start_write() {
    // проходим по файлам
    for (int i = 0; i < COUNT_OF_FILES; i++) {

        char* p_start = ((char*) array) + i * FILE_SIZE;
        char id[] = {'a' + i, '\0'};

        //размер данных, которые запишутся в файл
        int size = FILE_SIZE;
        if (i == COUNT_OF_FILES - 1 && MEMORY_SIZE % FILE_SIZE != 0)
            size = MEMORY_SIZE % FILE_SIZE;

        int file = open(id, O_CREAT | O_WRONLY, S_IRWXU | S_IRGRP | S_IROTH);
        writeToFileByBlock(p_start, file, size);

        close(file);
    }
    printf("start_write(): finished to write in files\n");
}

void start_reading_random_threads() {

    //выделяем память под массив потоков
    pthread_t *threads = (pthread_t *) malloc(THREADS_COUNT * sizeof(pthread_t));
    printf("start_reading_random_threads(): Allocate memory for threads\n");

    //запускаем потоки
    for (int i = 0; i < THREADS_COUNT; i++) {
        pthread_create(&(threads[i]), NULL, threadFunc, (void *) i);
    }

    //ожидаем выполнение потоков
    for (int i = 0; i < THREADS_COUNT; i++)
        pthread_join(threads[i], NULL);
    printf("start_reading_random_threads(): Closed all threads\n");
    free(threads);
    printf("start_reading_random_threads(): Free memory for threads\n");
}

// функция для потока, который считывает random -> array
void *threadFunc(void *i) {
    //номер потока
    int number = (int) i;
    printf("threadFunc(): Launch %d thread\n", number);
    //количество чисел, которые считывает каждый поток
    int byteCount = MEMORY_SIZE / THREADS_COUNT;
    //файл случайных чисел
    int random = open("/dev/urandom", O_RDONLY);

    // указатель куда записываем в массив
    char* p_start = ((char*) array) + number * byteCount;

    int readFlag;
    if (number == THREADS_COUNT - 1)
        readFlag = read(random, p_start, byteCount + MEMORY_SIZE % THREADS_COUNT);
    else
        readFlag = read(random, p_start, byteCount);

    if (readFlag == -1){
        printf("threadFunc(): %d thread cannot read from random to array\n", number);
    } else printf("threadFunc(): %d thread read %d bytes\n", number, readFlag);

    printf("threadFunc(): End %d thread\n", number);
}

long get_min(int *readArray, int size) {
    int min = MAX_INT;

    for (int i = 0; i < size; i++) {
        if (*(readArray + i) < min) {
            min = *(readArray + i);
        }
    }
    return min;
}

long get_average(int *readArray, int size) {
    long average = 0;

    for (int i = 0; i < size; i++) {
        average += *(readArray + i);
    }
    return average / size;
}

void *threadReadingFunc(void *number) {
    sem_wait(&semaphore);


//    FILE *files[COUNT_OF_FILES];

    for (int i = 0; i < COUNT_OF_FILES; i++) {

        //размер файла
        int size = FILE_SIZE;
        //последний файл может быть меньше
        if (i == COUNT_OF_FILES - 1 && MEMORY_SIZE % FILE_SIZE != 0)
            size = MEMORY_SIZE % FILE_SIZE;

        //выделяем память для прочитанной информации
        void *readInfo = malloc(size);

//        int countOfNumber = size/sizeof(int);

        char id[] = {'a' + i, '\0'};

        int file = open(id, O_RDONLY );
        read(file, readInfo, size);

        int *readArray = (int *) readInfo;
        int min = get_min(readArray, size / sizeof(int));

        printf("Минимальное значение %d", min);
        printf("\n");
        close(file);
        free(readInfo);
    }
    sem_post(&semaphore);
}

void writeToFileByBlock(char* p_start, int file, int size){

    int blockCount = size / BLOCK_SIZE;

    for (int i = 0; i < blockCount; i++){
        int writeFlag = write(file, p_start + i * BLOCK_SIZE, BLOCK_SIZE);
//        printf("writeToFileByBlock(): wrote %d bytes in %d file\n", writeFlag, file);
    }

    //Дописываем данные, которые не попали в блоки
    if (size % BLOCK_SIZE != 0) {
        int writeFlag = write(file, p_start+blockCount*BLOCK_SIZE, size % BLOCK_SIZE);
//        printf("writeToFileByBlock(): wrote %d bytes in %d file\n", writeFlag, file);
    }

    printf("writeToFileByBlock(): wrote in %d file\n", file);

}




