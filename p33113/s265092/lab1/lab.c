#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <syscall.h>
#include <semaphore.h>
#include <pthread.h>

//A=359;B=0x95B4347C;C=malloc;D=73;E=43;F=nocache;G=107;H=random;I=50;J=avg;K=sem

#define A 359
#define D 73
#define E 43
#define G 512
#define I 50
#define FILE_AMOUNT (A/E + (A % E == 0 ? 0 : 1))
#define RANDOM_DEV "/dev/urandom"

sem_t sem;
int random_fd;
double file_avg[8];
void *alloc_memory;

void *fill_memory(void* thread_addr){
    int bytes_per_thread = (A * 1024 * 1024)/D;
    int thread_num = (intptr_t) thread_addr;
    intptr_t start_addr = (intptr_t) alloc_memory + thread_num * bytes_per_thread;

    if (thread_num == D-1){
        bytes_per_thread += (A * 1024 * 1024) - bytes_per_thread*D;
    }

    ssize_t res = read(random_fd, (void*) start_addr, bytes_per_thread);

    if (res == -1){
        printf("Could not fill the memory at %p\n", (void*) start_addr);
        printf("errno = %d\n", errno);
        exit(-1);
    }
    return 0;
}

void memory_fill_init(){
    printf("Filling with random data, using %d threads.\n\n", D);

    random_fd = open(RANDOM_DEV, O_RDONLY);
    if (random_fd < 0){
        printf("Could not open %s\n", RANDOM_DEV);
        exit(1);
    }

    pthread_t threads[D];
    for (intptr_t i = 0; i < D; i++){
        pthread_create(&threads[i], 0, fill_memory, (void*) i);
    }
    for (int i = 0; i < D; i++){
      pthread_join(threads[i], NULL);
    }
    close(random_fd);
    printf("Filled with data.\n");
    getchar();
}

void file_write(char* filename, intptr_t start_addr, int size){
    int block_amount = size/G;
    if (G * block_amount < size){
        block_amount++;
    }

    int write_fd = open(filename, O_CREAT | O_WRONLY, S_IRWXU | S_IRGRP | S_IROTH);
    if (write_fd < 0){
        printf("Could not write in '%s'\n", filename);
	printf("errno = %d\n", errno);
        exit(-1);
    }

    sem_wait(&sem);
    for (int i = 0; i < block_amount; i++){
        int available_size = G;
        if (i == block_amount - 1){
            available_size = size - G * (block_amount - 1);
	}

        ssize_t bytes_written = write(write_fd, (void*) (start_addr + (rand() % block_amount)*G), available_size);
        printf("Writing in '%s': %d/%d blocks\r", filename, i, block_amount);
        fflush(stdout);
        if (bytes_written == -1){
            printf("\n");
            printf("Could not write in '%s'\n", filename);
	    printf("errno = %d\n", errno);
            exit(-1);
        }
    }

    sem_post(&sem);
    printf("Written in '%s': %d/%d blocks\r\n", filename, block_amount, block_amount);
    close(write_fd);
}

void file_write_init(){
    printf("Writing data from memory to files 0-8.\n");

    for (int i = 0; i < FILE_AMOUNT; i++){
        char filenames[] ={'0'+ i, '\0'};
        int file_size = E * 1024 * 1024;

        if (i == FILE_AMOUNT - 1){
            file_size = (A - E * (FILE_AMOUNT - 1)) * 1024 * 1024;
	}
        file_write(filenames, (intptr_t) alloc_memory + E * 1024 * 1024 * i, file_size);
    }
}

void* file_read(){
    for (int i = 0; i < FILE_AMOUNT; i++){
        char filenames[] ={'0'+ i, '\0'};
        int fd = open(filenames,O_CREAT | O_RDONLY, S_IRWXU | S_IRGRP | S_IROTH);
        if (fd < 0){
            printf("Could not read from '%s'.\n", filenames);
	    printf("errno = %d\n", errno);
            exit(-1);
        }

        sem_wait(&sem);
        off_t size = lseek(fd, 0L, SEEK_END);
        lseek(fd, 0, SEEK_SET);
        int8_t* read_int = (int8_t*) malloc(size);
        ssize_t bytes_read = read(fd, read_int, size);
        ssize_t nums = bytes_read/sizeof(int8_t); 
        sem_post(&sem);

        for (size_t j = 0; j < nums; j += 1){
            file_avg[i] += read_int[j];
        }
        file_avg[i] = (file_avg[i]/nums);
	printf("%f %d\n", file_avg[i], i);
        free(read_int);
        close(fd);
  	}
    return 0;
}

void file_read_init(){
    printf("Reading from files, using %d threads.\n", I);
    pthread_t threads[I];
    for (int i = 0; i < I; i++){
        pthread_create(&threads[i], NULL, file_read, NULL);
    }

    for (int i = 0; i < I; i++){
        pthread_join(threads[i], NULL);
    }	
}

void analysis_res(){
    for (int i = 0; i < FILE_AMOUNT; i++){
        printf("Average value in '%d' - %f.\n", i, file_avg[i]);
    }	
}

void free_memory(){
    free(alloc_memory);
    printf("Memory deallocated.\n");
    getchar();
}

int main(int argc, char *argv[]){
    sem_init(&sem, 0, 1);
    printf("Memory not allocated yet.\n");
    getchar();
    char cycle = '1';
    while(cycle == '1'){
        alloc_memory = malloc(A * 1024 * 1024);
        printf("Memory allocated.\n");
        getchar();
        memory_fill_init();
        puts( "Writing is executed in the main process.\n");
        file_write_init();
        free_memory();
        puts( "Started reading process.\n" );
        file_read_init();
        analysis_res();
        puts("Press 1 to continue the cycle.\n");
        scanf("%c", &cycle);
    }
    return 0;
}
