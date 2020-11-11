#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <semaphore.h>
//#define A 190*1024*1024
#define D 29
int E = 91*1000*1000;
int MAX_INT = 2147483647;
//#define G 33
int A = 190*1000*1000;
int I = 17;
//структура для данных потока
typedef struct{
    int* array;
    int number;
} pthrData;

// семафора
sem_t semaphore;

void* threadFunc(void* threadData){
    pthrData* data = (pthrData*) threadData;
    int index = (A/sizeof(int))/D;
    //файл случайных чисел
    int random = open("/dev/urandom", O_RDONLY);
    if (data->number != D - 1) {
        for (int i = data->number * index; i < ((data->number) + 1) * index; i++)
            read(random, &data->array[i], sizeof(int));
    } else {
        for (int i = data->number * index; i < A / sizeof(int); i++)
            read(random, &data->array[i], sizeof(int));
    }

}

long get_min(int* array, int size){
    int min = MAX_INT;


    for (int i = 0; i < size; i++){
        if (*(array+i) < min ){
            min = *(array+i);
        }
    }
    return min;
}

long get_average(int* array, int size){
    long average = 0;


    for (int i = 0; i < size; i++){
        average += *(array+i);
    }
    return average/size;
}
void* iThreadFunc(void * number){
    sem_wait(&semaphore);

    int n = (int*) number;
//    printf("Working ");
//    printf("%d", n);
//    printf("thread");
//    printf("\n");
    int countOfFiles = A/E;
    FILE* files[countOfFiles];
    for (int i = 0; i < countOfFiles; i++) {
        void *readInfo = malloc(E);
        char id[100] = "";
        sprintf(id, "%d", i);
        strcat(id, "file");
        files[i] = fopen(id, "r");
        int a = fread(readInfo, sizeof(int), E / sizeof(int), files[i]);
        if (a != -1) {
            int *readArray = (int *) readInfo;
            int min = get_average(readArray, E/sizeof(int));
            printf("%d",min);
            printf("\n");
//            printf("readInfo: ");
//            for (int j = 0; j < 10; j++) {
//                printf("%d", readArray[j]);
//                printf(" | ");
//            }
//            printf("\n");
//            printf("Thread ");
//            printf("%d", n);
//            printf(" trying to close file ");
//            printf("%d", i);
//            printf("\n");
            fclose(files[i]);
//            printf("Thread ");
//            printf("%d", n);
//            printf(" closed file ");
//            printf("%d", i);
//            printf("\n");
        }
        else{
            printf("fread is broken");
        }

        free(readInfo);


    }
//    printf("\n");
//    printf("Done ");
//    printf("%d", n);
//    printf("thread");
//    printf("\n");
    sem_post(&semaphore);
}

int main(){
    printf("Before al");
    sleep(10);
    for (int k = 0; 1; k++) {
        printf("We have done it ");
        printf("%d", k);
        printf("\n");
        //выделяем память
        int *array = malloc(A);
        printf("After al");
        sleep(10);
        int N = A / sizeof(int);
        for (int i = 0; i < N; i++) {
            array[i] = 0;
        }


        //выделяем память под массив потоков
        pthread_t *threads = (pthread_t *) malloc(D * sizeof(pthread_t));
        //и для сруктур с данными потоков
        pthrData *threadData = (pthrData *) malloc(D * sizeof(pthrData));

        //запускаем потоки
        for (int i = 0; i < D; i++) {
            threadData[i].array = array;
            threadData[i].number = i;
            pthread_create(&(threads[i]), NULL, threadFunc, &threadData[i]);
        }
        //ожидаем выполнение потоков
        for (int i = 0; i < D; i++)
            pthread_join(threads[i], NULL);
//    for (int i = 0; i < 50; i++){
//        printf("%d", array[i]);
//        printf("\n");
//    }
        printf("After data");
        sleep(10);
        free(threads);
        free(threadData);

//    int countOfFiles = A/E;
//    int files[countOfFiles];
//    for (int i = 0; i < countOfFiles; i++){
//        char id[100] = "";
//        sprintf(id, "%d", i);
//        strcat(id, "file");
//        creat(id, S_IRWXU);
//        files[i] = open(id, O_WRONLY);
//        write(files[i], array+i*(E/sizeof(int)),  E);
//        if (i == countOfFiles-1){
//            printf("%zd", write(files[i], array,  E));
//            printf("\n");
//        }
//        else {
//            printf("%zd", write(files[i], array, E));
//            printf("\n");
//        }
//        close(files[i]);
//        void* readInfo = malloc(E);
//        printf("%d", files[i] = open(id, O_RDONLY));
//        printf("\n");
//        printf("%zd", read(files[i], readInfo, E));
//        printf("\n");
//        int* readArray = (int *) readInfo;
//        printf("%lu", sizeof(readInfo));
//        printf("\n");
//        printf("readInfo: ");
//        for(int j = 0; j < 500; j++){
//            printf("%d", readArray[j]);
//            printf(" | ");
//        }
//        printf("\n");


//    }

        int countOfFiles = A / E;
        FILE *files[countOfFiles];
        for (int i = 0; i < countOfFiles; i++) {
            char id[100] = "";
            sprintf(id, "%d", i);
            strcat(id, "file");
            files[i] = fopen(id, "w");
            fwrite(array + i * E / sizeof(int), sizeof(int), E / sizeof(int), files[i]);
            fclose(files[i]);
        }

        // семафор для потоков - 0
        sem_init(&semaphore, 0, 2);

        pthread_t *iThreads = (pthread_t *) malloc(I * sizeof(pthread_t));
        //запускаем потоки
        for (int i = 0; i < I; i++) {
            pthread_create(&(iThreads[i]), NULL, iThreadFunc, (void *) i);
        }
        //ожидаем выполнение потоков
        for (int i = 0; i < I; i++)
            pthread_join(iThreads[i], NULL);
        sem_destroy(&semaphore);

        free(array);
        printf("After deal");
        sleep(10);
    }

    return 0;
}
