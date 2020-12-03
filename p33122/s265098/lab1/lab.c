
#define _GNU_SOURCE
#include <sys/mman.h>
#include <stdio.h>
#include <limits.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h> 
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <fcntl.h>
#include <string.h>
//A=162; B=0x5931EDDF; C=mmap; D=25; E=34; F=nocache; G=30; H=seq; I=75; J=sum; K=cv;

#define B_START_ADDRESS 0x5931EDDF
#define A_MB 162
#define D_THREAD_NUM 25
#define E_MB_FILE_SIZE 34
#define G_BYTES_BLOCK_SIZE 30
#define I_READ_THREAD_NUM 75

int A_BYTES, outputFD;
FILE* randomf;
unsigned char *address, *ptr;
int readCondition = 1;
int writeCondition = 1;

typedef struct {
    unsigned char *address;
    int bufSize;
} FillMemoryParams;

struct FlexParams{
    pthread_mutex_t mutex;
    pthread_cond_t condVar;
};

typedef struct {
    int filesNum;
    unsigned long long fileSize;
    pthread_mutex_t *mutexes;
    pthread_cond_t *condVars;
} FillFilesParams;

typedef struct {
    int filesNum;
    pthread_mutex_t *mutexes;
    pthread_cond_t *condVars;
} ReadFilesParams;


void* threadFillMemoryFunc(void* fillMemoryParams){
    FillMemoryParams *fmp = (FillMemoryParams *) fillMemoryParams;
    fread((void *) fmp->address, 1, fmp->bufSize, randomf);
    return NULL;
}

void* threadFillFilesFunc(void* args ){
    //printf("threadFillFilesFunc entered\n");
    FillFilesParams *ffp = args;
    int fileFD, blockSize, blocksNum;;
    unsigned char *addressf;
    char filename[8];

    struct stat buff;
    do{
        for(int i = 0; i < ffp->filesNum; ++i){
            //printf("Filling file #%d\n", i);
            pthread_mutex_lock(&ffp->mutexes[i]);
            sprintf(filename, "file_%d", i);
            fileFD = open(filename, O_CREAT | O_WRONLY | O_DIRECT | O_TRUNC, __S_IREAD | __S_IWRITE);

            stat(filename, &buff);
            blockSize = (int) buff.st_blksize;
            blocksNum = E_MB_FILE_SIZE * 1024 * 256 * sizeof(int) / blockSize;

            char *block = (char *) malloc(2 * blockSize - 1);
            char *wblock = (char *) (((uintptr_t) block + blockSize - 1) & ~((uintptr_t) blockSize - 1));
            
            for(int j = 0; j < blocksNum; ++j){
                addressf = ptr + i * ffp->fileSize + blockSize * j;
                memcpy(wblock, addressf, blockSize);

                if(pwrite(fileFD, wblock, blockSize, j*blockSize) == -1){
                    printf("pwrite failed");
                    close(fileFD);
                    free(block);
                }
            }
            close(fileFD);
            free(block);
            pthread_cond_broadcast(&ffp->condVars[i]);
            pthread_mutex_unlock(&ffp->mutexes[i]);
        }
    }while(writeCondition);
    return NULL;
}

void* threadReadFilesFunc(void *args){
    //printf("threadReadFilesFunc entered\n");
    ReadFilesParams *rfp = args;
    char filename[8];
    long long blocksNum = E_MB_FILE_SIZE*1000*1000/G_BYTES_BLOCK_SIZE;
    do{
        for(int i = 0; i < rfp->filesNum; ++i){
            printf("reading file #%d\n", i);
            long long sum = 0;
            printf("before sprintf\n");
            sprintf(filename, "file_%d", i);
            printf("before lock\n");
            if(!readCondition){
                break;
            }
            if(!pthread_mutex_trylock(&rfp->mutexes[i])){
                continue;
            }
            printf("waiting cond for file #%d\n", i);
            pthread_cond_wait(&rfp->condVars[i], &rfp->mutexes[i]);
            
            printf("stop waiting cond for file #%d\n", i);
            FILE *file = fopen(filename, "rb");
            char buffer[G_BYTES_BLOCK_SIZE];
            for(long long j = 0; j < blocksNum; ++j){
                if (fread(&buffer, 1, G_BYTES_BLOCK_SIZE, file) != G_BYTES_BLOCK_SIZE){
                    continue;
                }
                sum += atoi(buffer);
            }
            printf("Sum from file #%d = %lld\n", i, sum);
            fclose(file);
            pthread_mutex_unlock(&rfp->mutexes[i]);
        }
    }while(readCondition);
    return NULL;
}


void fillMemory(){
    printf("    Memory filling...\n");
        
    int bufSize = A_BYTES / D_THREAD_NUM;
    int lastThreadBufSize = A_BYTES % D_THREAD_NUM + bufSize;

    pthread_t threads[D_THREAD_NUM];
    
    for(int i = 0; i < D_THREAD_NUM - 1; ++i){
        FillMemoryParams fillMemoryParams;
        fillMemoryParams.address = ptr; 
        fillMemoryParams.address += bufSize * i;
        fillMemoryParams.bufSize = bufSize;
        pthread_create(&threads[i], NULL, threadFillMemoryFunc, &fillMemoryParams);
        
    }
    FillMemoryParams fillMemoryParams;
    fillMemoryParams.address = ptr;
    fillMemoryParams.address += bufSize * (D_THREAD_NUM - 1);
    fillMemoryParams.bufSize = lastThreadBufSize;
    pthread_create(&threads[D_THREAD_NUM - 1], NULL, threadFillMemoryFunc, &fillMemoryParams);

    for(int i = 0; i < D_THREAD_NUM; ++i){
        pthread_join(threads[i], NULL);
    }

    printf("After memory filling\n");
    getchar();
}
void fillFiles(){
    printf("    Files filling...\n");

    FillFilesParams ffp;
    ffp.fileSize = E_MB_FILE_SIZE * 1000 * 1000;
    ffp.filesNum = A_MB / E_MB_FILE_SIZE;
    //printf("ffp created\n");

    ReadFilesParams rfp;
    rfp.filesNum = ffp.filesNum;
    //printf("rfp created\n");
    
    // pthread_cond_t condVars[ffp.filesNum];
    // pthread_mutex_t mutexArr[ffp.filesNum];
    pthread_cond_t *condVars = malloc(sizeof(pthread_cond_t) * ffp.filesNum);
    pthread_mutex_t *mutexArr = malloc(sizeof(pthread_mutex_t) * ffp.filesNum);
    //printf("Initializing mutexes and cond vars\n");
    for(int i = 0; i < ffp.filesNum; ++i){
        pthread_mutex_init(&mutexArr[i], NULL);
        pthread_cond_init(&condVars[i], NULL);
        ffp.condVars = condVars;
        ffp.mutexes = mutexArr;

        rfp.condVars = condVars;
        rfp.mutexes = mutexArr;
    }

    
    pthread_t fileFillerThread;
    pthread_create(&fileFillerThread, NULL, threadFillFilesFunc, &ffp);
    //printf("file fillers created\n");
    
    
    pthread_t readFilesThreads[I_READ_THREAD_NUM];
    for(int i = 0; i < I_READ_THREAD_NUM; i++){
        pthread_create(&readFilesThreads[i], NULL, threadReadFilesFunc, &rfp);
    }
    //printf("file readers created\n");

    sleep(1);
    printf("Press key to stop\n");
    getchar();
    readCondition = 0;

    for(int i = 0; i < I_READ_THREAD_NUM; i++){
        pthread_join(readFilesThreads[i], NULL);
    }
    printf("file readers joined\n");

    writeCondition = 0;
    pthread_join(fileFillerThread, NULL);
    printf("file filler joined\n");

    free(condVars);
    free(mutexArr);
}

int main(){
    A_BYTES = A_MB * 1000 * 1000;
    address = (unsigned char *) B_START_ADDRESS;
    randomf = fopen("/dev/urandom", "r");

    printf("Before memory allocation\n");
    getchar();
    
    // ptr = mmap((void *) B_START_ADDRESS, A_BYTES, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    ptr = mmap(NULL, A_BYTES, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if(ptr == MAP_FAILED){
            printf("Mapping Failed\n");
            return -1;
    }

    printf("After memory allocation\n");
    getchar();    
    fillMemory();
    fillFiles();

    if(munmap(ptr, A_BYTES) == -1){
        printf("Munmap failed\n");
            return -1;
    }

    fclose(randomf);  

    printf("After memory deallocation\n");
    getchar();

    return 0;
}