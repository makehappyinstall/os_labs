#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <stdatomic.h>
#include <syscall.h>
#include <string.h>
#include <sys/file.h>

#define megabyte_size 1024*1024
#define A 184
#define D 131
#define E 72
#define G 117
#define I 28
#define FILES_NUMBER (A/E + (A % E == 0 ? 0 : 1))
#define RANDOM_SRC "/dev/urandom"

int infinity = 1;
int infinity2 = 1;
void *memory_region;
int randomFd;

void *fill_with_random(void* vargPtr) {
    int chunkSize = (A * megabyte_size)/D;
    int threadIndex = (intptr_t) vargPtr;
    intptr_t ptrStart = (intptr_t)memory_region + threadIndex * chunkSize;
    void *start = (void *)ptrStart;
    if (threadIndex == D - 1)
        chunkSize += (A*megabyte_size) - ((A*megabyte_size)/D)*D;
    ssize_t result = read(randomFd, start, chunkSize);
    if (result == -1) {
        printf("Не получилось заполнить область с %p, размер = %d\n",(void *)ptrStart, chunkSize);
        printf("errno = %d\n", errno);
        exit(-1);
    }
    return 0;
}

void write_to_memory() {
    printf("Заполнение случайными числами в %d потоков\n", D);
    randomFd = open(RANDOM_SRC, O_RDONLY);
    if (randomFd < 0) {
        printf("Не получилось открыть %s\n", RANDOM_SRC);
        exit(1);
    }
    pthread_t threads[D];
    for (int i = 0; i < D; i++) {
        pthread_create(&threads[i], 0, fill_with_random, (void*) (intptr_t)i);
    }
    for (int i = 0; i < D; i++){
      pthread_join(threads[i], NULL);
    }
    close(randomFd);
    printf("После заполнения участка данными ([ENTER])\n");
    getchar();

}

void writeSingleFile(char* fileName, intptr_t start, int size) {
    int blocksNumber = size/G;
    if (G*blocksNumber < size)
        blocksNumber++;
    int fd = open(fileName, O_CREAT | O_WRONLY , S_IRWXU | S_IRGRP | S_IROTH);
    if (fd < 0) {
        printf("Не получилось открыть файл '%s' для записи\n", fileName);
        exit(-1);
    }
    flock(fd, LOCK_EX);
    for (int i = 0; i < blocksNumber; i++) {
        int effectiveBlockSize = G;
        if (i == blocksNumber - 1)
            effectiveBlockSize = size - G*(blocksNumber - 1);
        intptr_t writeStartPtr = start + i*G;
        ssize_t wroteBytes = write(fd, (void *)writeStartPtr, effectiveBlockSize);
        printf("Записывается файл '%s': %d/%d блоков\r", fileName, i, blocksNumber);
        fflush(stdout);
        if (wroteBytes == -1) {
            printf("\n");
            printf("Не получилось записать файл '%s'\n", fileName);
            exit(-1);
        }
    }
    flock(fd, LOCK_UN);
    close(fd);
    printf("Файл записался!\n");
}

void write_to_file() {
    printf("Запись данных из памяти в файлы...\n");
    for (int i = 0; i < FILES_NUMBER; i++) {
        char fileName[] = {'0' + i, '\0'};
        int actualSize = E*megabyte_size;
        if (i == FILES_NUMBER - 1)
        actualSize = (A - E*(FILES_NUMBER - 1))*megabyte_size;
	intptr_t start = (intptr_t)memory_region + E * i;
        writeSingleFile(fileName, start, actualSize);
    }
}

void free_mem(){
  free(memory_region);
  printf("После деаллокации ([ENTER])\n");
  getchar();
}


void *filesAnalyzeThread(void* vargPtr) {
  	for (int i = 0; i < FILES_NUMBER; i++) {
  	    char fileName[] = {'0' + i, '\0'};
        int fd = open(fileName, O_CREAT | O_RDONLY, S_IRWXU | S_IRGRP | S_IROTH);
        if (fd < 0) {
            printf("Не получилось открыть файл '%s' для чтения\n", fileName);
            exit(-1);
        }
        flock(fd, LOCK_SH);
        off_t size = lseek(fd, 0L, SEEK_END);
        lseek(fd, 0, SEEK_SET);
        __int8_t* data = (__int8_t*) malloc(size);
        ssize_t readBytes = read(fd, data, size);
        flock(fd, LOCK_UN);
        close(fd);
        __int64_t sum = 0;
        for (size_t i = 0; i < readBytes/sizeof(__int8_t); i += 1) {
          sum += data[i];
        }
        printf("Анализ: Сумма в файле '%s' - %ld\n",fileName, sum);
        free(data);
  	}
    return 0;
}

void read_from_file(){
  
    printf("Анализ содержимов файлов, используя %d потоков...\n", I);
    pthread_t threads[I];
    for (int i = 0; i < I; i++) {
       pthread_create(&threads[i], NULL, filesAnalyzeThread, NULL);
	  }
    for (int i = 0; i < I; i++) {
       pthread_join(threads[i], NULL);
    }
  
	
}

int main(int argc, char *argv[]){
    printf("До аллокации ([ENTER])\n");
    getchar();
    memory_region = malloc(A * megabyte_size);
      printf("После аллокации ([ENTER])\n");
      getchar();
      write_to_memory();
      int pid = fork();
      if ( pid == 0 ) {
    		printf( "This is being printed from the child process\n" );
        	read_from_file();
    	} else {
    		printf( "This is being printed in the parent process:\n");
        	write_to_file();
		free_mem();
    	}
    
    return 0;
}
