/**
 * Variant:
 * A=133;B=0x206746DB;C=malloc;D=106;E=147;F=nocache;G=29;H=seq;I=133;J=min;K=sem
 **/

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>
#include <semaphore.h>
#include <getopt.h>
#include <stdnoreturn.h>

#define MALLOC_SIZE 132999680 // 133 MB
#define FILE_SIZE 146999808 // 147 MB
#define RANDOM_SRC "/dev/urandom"
#define FILL_THREADS 106
#define FILES_NUMBER (MALLOC_SIZE / FILE_SIZE + (MALLOC_SIZE % FILE_SIZE == 0 ? 0 : 1))
#define BLOCK_SIZE 512 // not 29 because of nocache
#define ANALYZING_THREADS 133


void* region_ptr;
int random_fd;
sem_t semaphore;

void* fill_thread_handler(void* varg_ptr) {
    int chunk_size = MALLOC_SIZE / FILL_THREADS;
    const int thread_index = (int) varg_ptr;
    void* ptr_start = region_ptr + thread_index * chunk_size;

    if (thread_index == FILL_THREADS - 1) {
        chunk_size += MALLOC_SIZE - chunk_size * FILL_THREADS;
    }

    int result = read(random_fd, ptr_start, chunk_size);

    if (result == -1) {
        fprintf(stderr, "ERROR!!! Can't fill memory for pointer='%p' with size='%d', errno='%d'\n", ptr_start, chunk_size, errno);
        exit(-1);
    }
}

void generate_data() {
    printf("Generating data started...\n");
    
    region_ptr = malloc(MALLOC_SIZE);
    
    if (region_ptr == NULL) {
        fprintf(stderr, "Failed to perform malloc, errno='%d'\n", errno);
        exit(-1);
    } else {
        printf("Memory allocated for address='%p'\n", region_ptr);
    }

    printf("Filling by random numbers using %d threads\n", FILL_THREADS);

    random_fd = open(RANDOM_SRC, O_RDONLY);
    
    if (random_fd < 0) {
        fprintf(stderr, "ERROR!!! Failed to open file='%s', errno='%d'\n", RANDOM_SRC, errno);
        exit(-1);
    }

    pthread_t threads[FILL_THREADS];

    for (int i = 0; i < FILL_THREADS; i++) {
        pthread_create(&threads[i], 0, fill_thread_handler, (void*) i);
    }

    printf("Waiting for all threads finished...\n");

    for (int i = 0; i < FILL_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    close(random_fd);
    printf("OK!!! Memory is filled!\n");
}

void write_single_file(char* file_name, void* start, int size) {
    int blocks_number = size / BLOCK_SIZE;
    
    if (BLOCK_SIZE * blocks_number < size) {
        blocks_number++;
    }

    int fd = open(file_name, O_CREAT | O_WRONLY | O_DIRECT, S_IRWXU | S_IRGRP | S_IROTH);

    if (fd < 0) {
        fprintf(stderr, "ERROR!!! Failed to open file='%s' for writing, errno='%d'\n", file_name, errno);
        exit(-1);
    }

    sem_wait(&semaphore);

    for (int i = 0; i < blocks_number; i++) {
        int effective_block_size = BLOCK_SIZE;
        
        if (i == blocks_number - 1) {
            effective_block_size = size - BLOCK_SIZE * (blocks_number - 1);
        }

        void* write_start_ptr = start + i * BLOCK_SIZE;

        int wrote_bytes = write(fd, write_start_ptr, effective_block_size);
        printf("Writing file='%s' in progress: %d/%d blocks\r", file_name, i, blocks_number);
        fflush(stdout);

        if (wrote_bytes == -1) {
            fprintf(stderr, "\nERROR!!! Failed to write file='%s', errno='%d'", file_name, errno);
            exit(-1);
        }
    }

    sem_post(&semaphore);
    close(fd);

    printf("File is written!\n");
}

void write_data() {
    printf("Writing data to file\n");

    for (int i = 0; i < FILES_NUMBER; i++) {
        char file_name[] = {'0' + i, '\0'};

        int actual_size = FILE_SIZE;
        if (i == FILES_NUMBER - 1) {
            actual_size = MALLOC_SIZE - FILE_SIZE * (FILES_NUMBER - 1);
        }

        write_single_file(file_name, region_ptr + FILE_SIZE * i, actual_size);
    }

    printf("OK!!! Writing data is done!\n");    
}

void free_data() {
    free(region_ptr);
}

void analyze_file(char* file_name) {
    int fd = open(file_name, O_CREAT | O_RDONLY, S_IRWXU | S_IRGRP | S_IROTH);

    if (fd < 0) {
        printf(stderr, "WARNING!!! Can't open file='%s' for reading!\n", file_name);
        return;
    }

    sem_wait(&semaphore);

    off_t size = lseek(fd, 0L, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    u_int8_t* data = (u_int8_t*) malloc(size);
    int read_bytes = read(fd, data, size);

    sem_post(&semaphore);
    close(fd);

    u_int8_t min = data[0];
    for (int i = 1; i < read_bytes / sizeof(u_int8_t); i++) {
        if (data[i] < min)
            min = data[i];
    }

    printf("Analyze report: Min 8-byte unsigned number in file='%s' is %d\n", file_name, min);

    free(data);
}

noreturn void* file_analyze_thread_handler(void* varg_ptr) {
    while(1) {
        for (int i = 0; i < FILES_NUMBER; i++) {
            char file_name[] = {'0' + i, '\0'};

            analyze_file(file_name);
        }
    }
}

void start_background_reading() {
    printf("Analyzing files using %d threads...\n", ANALYZING_THREADS);

    pthread_t threads[ANALYZING_THREADS];
    for (int i = 0; i < ANALYZING_THREADS; i++) {
        pthread_create(&threads[i], NULL, file_analyze_thread_handler, NULL);
    }
}



int main() {
    sem_init(&semaphore, 0, 1);

    start_background_reading();

    while(1) {
        generate_data();
        write_data();
        free_data();
    }
}
