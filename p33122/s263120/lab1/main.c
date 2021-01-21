#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <memoryapi.h>
#include <semaphore.h>






#define A 18 
#define B 0x37C28270 
#define D 93 
#define E 135 
#define G 75 
#define I 17 
int threadsActivated = 1;



int randRightVersion() {

    int retval;
    int *myInt = (int *) malloc(sizeof(int)); 
    *(((char *) myInt) + 0) = rand();
    *(((char *) myInt) + 1) = rand();
    *(((char *) myInt) + 2) = rand();
    *(((char *) myInt) + 3) = rand() % 127;
    retval = *myInt;
    free(myInt); 
    return retval;
}


typedef struct {
    char *memoryRegion;
    int numCount; 
} generatorParams;


void *threadGenerator(void *funcParams) {
    generatorParams *params = (generatorParams *) funcParams;
    int first_run = 1;
    do {
        for (int i = 0; i < params->numCount * 4; ++i) {
            params->memoryRegion[i] = rand();
        }
        if (first_run) {
            first_run = 0;
        }
    } while (threadsActivated);
    return NULL;
}


typedef struct {
    int filesNumber;
    sem_t *semaphore; 
    int *start; 
} fileWriterParams;


void *threadFileWriter(void *funcParams) {
    fileWriterParams *params = (fileWriterParams *) funcParams;
    char *thread_start_write = (char *) params->start;
    do {
        for (int i = 0; i < params->filesNumber; i++) {
            sem_wait(params->semaphore + i);
            char filename[8] = "malab_\0";
            filename[5] = 'A' + i;
            int blocksAmount = A * 1024 * 1024 / G;
            FILE *currentFile;
            fopen_s(&currentFile, filename, "w+");
            for (int d = 0; d < E * 1024 * 1024 / (G); d++) {
                fwrite(thread_start_write + ((randRightVersion() % blocksAmount) * G), G, 1, currentFile);
            }
            fclose(currentFile);

            sem_post(params->semaphore + i);
        }
    } while (threadsActivated);
    return NULL;
}


typedef struct {
    int id;
    int fileNumber;
    sem_t *semaphore;
    int futexCompare;
} fileReaderParams;


void *threadFileReader(void *funcParams) {
    fileReaderParams *params = (fileReaderParams *) funcParams;
    char filename[8] = "malab_\0"; 
    filename[5] = 'A' + params->fileNumber;

    do {
        sem_wait(params->semaphore);
        double sum = 0;
        long file_size = 0;
        FILE *current_file;
        fopen_s(&current_file, filename, "r");
        fseek(current_file, 0, SEEK_END);
        file_size = ftell(current_file);
        //int blocks = file_size/16/ (G);
        int blocks = 10;
        char *buffer = (char *) malloc(G);
        for (int i = 0; i < blocks * 2; i++) {
            int x = randRightVersion() % 10;

            fseek(current_file, x % (blocks) * G, SEEK_SET);
            fread(buffer, G, 1, current_file);
            for (int d = 0; d < G; d++) {
                sum += buffer[d];

            }
        }
        fclose(current_file);
        sum /= (G * blocks * 2);  
        printf("%d %.0f (0x%08x) random avg %s\n", params->id, sum, &sum, filename);
        free(buffer);
        sem_post(params->semaphore);
    } while (threadsActivated);
    return NULL;
}

int main() {

    printf("Before memory allocation\n");
    getchar();
    int *start = (int *) VirtualAlloc(B, A * 1024 * 1024, MEM_COMMIT, PAGE_READWRITE); 
    if (start == NULL) {

        start = (int *) VirtualAlloc(NULL, A * 1024 * 1024, MEM_COMMIT, PAGE_READWRITE); 
    }
    printf("After memory allocation");
    getchar(); 
    pthread_t *generators = (pthread_t *) VirtualAlloc(NULL, D * sizeof(pthread_t), MEM_COMMIT, PAGE_READWRITE);
    generatorParams *generatorsData = (generatorParams *) VirtualAlloc(NULL, D * sizeof(generatorParams), MEM_COMMIT,
                                                                       PAGE_READWRITE);
    pthread_t *writers = (pthread_t *) VirtualAlloc(NULL, 1 * sizeof(pthread_t), MEM_COMMIT, PAGE_READWRITE);
    fileWriterParams *writersParams = (fileWriterParams *) VirtualAlloc(NULL, sizeof(fileWriterParams), MEM_COMMIT,
                                                                        PAGE_READWRITE);
    pthread_t *readers = (pthread_t *) VirtualAlloc(NULL, I * sizeof(pthread_t), MEM_COMMIT, PAGE_READWRITE);
    fileReaderParams *readersParams = (fileReaderParams *) VirtualAlloc(NULL, I * sizeof(fileReaderParams), MEM_COMMIT,
                                                                        PAGE_READWRITE);

    writersParams->filesNumber = A / E;
    if (A % E > 0)writersParams->filesNumber++;

    sem_t *sems = (sem_t *) VirtualAlloc(NULL, writersParams->filesNumber * sizeof(sem_t), MEM_COMMIT,
                                         PAGE_READWRITE);

    for (int i = 0; i < writersParams->filesNumber; i++) {
        sem_init(sems[i], 0, 0);
    }
    int *tmpointer = start;
    int amount = A * 1024 * 1024 / D / sizeof(int);
    for (int i = 0; i < D; i++) {

        generatorsData[i].memoryRegion = (char *) tmpointer;
        tmpointer += amount;
        generatorsData[i].numCount = amount;
    }
    generatorsData[D - 1].numCount += (A * 1024 * 1024 / sizeof(int)) % D;


    writersParams->semaphore = &sems[0];
    writersParams->start = start;
    for (int i = 0; i < I; i++) {
        readersParams[i].fileNumber = (i + 1) % writersParams->filesNumber;
        readersParams[i].semaphore = &sems[readersParams[i].fileNumber];
        readersParams[i].id = i;
    }
    for (int i = 0; i < D; i++) {
        pthread_create(&generators[i], NULL, threadGenerator, &generatorsData[i]);
    }
    pthread_create(writers, NULL, threadFileWriter, writersParams);
    for (int i = 0; i < I; i++) {
        pthread_create(&readers[i], NULL, threadFileReader, &readersParams[i]);
    }
    getchar();
    printf("Deactivate\n");
    threadsActivated = 0;
    for (int i = 0; i < D; i++)
        pthread_join(generators[i], NULL);
    for (int i = 0; i < D; i++) {
        sem_post(*readersParams[i].semaphore);
        pthread_join(readers[i], NULL);
    }
    pthread_join(writers, NULL);
    VirtualFree(start, A * 1024 * 1024, MEM_RELEASE);
    VirtualFree(generators, D * sizeof(pthread_t), MEM_RELEASE);
    VirtualFree(generatorsData, D * sizeof(generatorParams), MEM_RELEASE);
    VirtualFree(writers, 1 * sizeof(pthread_t), MEM_RELEASE);
    VirtualFree(writersParams, 1 * sizeof(fileWriterParams), MEM_RELEASE);
    VirtualFree(readers, 1 * sizeof(pthread_t), MEM_RELEASE);
    VirtualFree(readersParams, 1 * sizeof(fileReaderParams), MEM_RELEASE);
    VirtualFree(sems, I * sizeof(sem_t), MEM_RELEASE);
}
