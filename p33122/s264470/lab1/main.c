#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include "memory.h"

#define MEM_SIZE 118*1024*1024
#define FIRST_ADDRESS 0x3C28E4CC
#define NUM_THREADS_MEMORY 116
#define NUM_THREADS_FILE 22
#define FILE_SIZE 14*1024*1024
#define BLOCK_SIZE 23
#define RANDOM_FILE_NAME "/dev/urandom"
#define FULL_FILES_COUNT ((MEM_SIZE) / (FILE_SIZE))

typedef struct{
    int thread_num;
    int * memory_region;
    int start_number;
    int end_number;
    int fd;
}memory_fill_params;

typedef struct{
    int * memory_region;
}file_write_params;

typedef struct{
    int file_number;
    int thread_number;
}file_read_params;

int cycle_stop = 0;
int min_value = INT_MAX;
//sem_t semaphore_urandom;
sem_t semaphore_file[FULL_FILES_COUNT+1];

void write_to_file(const int * memory_region, int start, int fd, int file_size){
    int num_blocks = file_size / BLOCK_SIZE;
    char * buffer = (char *) malloc(BLOCK_SIZE);
    char * v_memory = (char *) memory_region + start;
    for(int i = 0; i <= num_blocks * 2; i++){
        int block_number = rand() % (num_blocks + 1);
        int start_byte = block_number * BLOCK_SIZE;
        int block_size = block_number == num_blocks ? file_size - (BLOCK_SIZE*num_blocks) : BLOCK_SIZE;
        memcpy(buffer, v_memory + start_byte, block_size);
        pwrite(fd, buffer, block_size, start_byte);
    }
    free(buffer);
}

void read_from_file(int * memory_region, int fd, int file_size){
    int num_blocks = file_size / BLOCK_SIZE;
    char * buffer = (char *) malloc(BLOCK_SIZE);
    char * v_memory = (char *) memory_region;
    for(int i = 0; i <= num_blocks * 2; i++){
        int block_number = rand() % (num_blocks + 1);
        int start_byte = block_number * BLOCK_SIZE;
        int block_size = block_number == num_blocks ? file_size - (BLOCK_SIZE*num_blocks) : BLOCK_SIZE;
        pread(fd, buffer, block_size, start_byte);
        memcpy(v_memory + start_byte, buffer, block_size);
    }
    free(buffer);
}

void* fill_memory_thread(void * params_void){
    memory_fill_params * params = (memory_fill_params *) params_void;
    printf("[Generator %i] start\n", params->thread_num);
    char * memory_segment = (char *) params->memory_region + (params->start_number * 4);
    int size = (params->end_number - params->start_number) * 4;
    do{
        pread(params->fd, memory_segment, size, 0);
    } while (!cycle_stop);
    printf("[Generator %i] finish\n", params->thread_num);
    return NULL;
}

void* file_write_thread(void * params_void){
    file_write_params * params = (file_write_params *) params_void;
    printf("[Writer] start\n");
    int first_run = 1;
    do{
        //заполняем файлы
        for(int i=0; i<=FULL_FILES_COUNT; i++){
            char filename[16];
            sprintf(filename, "lab1_file_%i.bin", i);
            if(!first_run){
                sem_wait(&semaphore_file[i]); //при первом запуске семафор захвачен заранее
            }
            int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 00666);
            if(fd == -1){
                printf("[Writer] can not open file %i\n", i);
                exit(-1);
            }
            //в последний файл сложим остатки
            int file_size = i == FULL_FILES_COUNT ? MEM_SIZE - (FULL_FILES_COUNT * FILE_SIZE) : FILE_SIZE;
            write_to_file(params->memory_region, FILE_SIZE/4*i, fd, file_size);
            close(fd);
            sem_post(&semaphore_file[i]);
        }
        if(first_run){
            first_run = 0;
        }
    } while (!cycle_stop);
    printf("[Writer] finish\n");
    return NULL;
}

void* file_read_thread(void * params_void){
    file_read_params * params = (file_read_params *) params_void;
    printf("[Reader %i] start with file %i\n", params->thread_number, params->file_number);
    do{
        //заполняем файлы
        char filename[16];
        sprintf(filename, "lab1_file_%i.bin", params->file_number);
        sem_wait(&semaphore_file[params->file_number]);
        int fd = open(filename, O_RDONLY);
        if(fd == -1){
            sem_post(&semaphore_file[params->file_number]);
            printf("[Reader %i] can not open file %i\n", params->thread_number, params->file_number);
            return NULL;
        }
        //последний файл короче остальных
        int file_size = params->file_number == FULL_FILES_COUNT ? MEM_SIZE - (FULL_FILES_COUNT * FILE_SIZE) : FILE_SIZE;
        int * memory = (int *) malloc(file_size);
        read_from_file(memory, fd, file_size);
        close(fd);
        sem_post(&semaphore_file[params->file_number]);
        for(int i=0; i<file_size/4; i++){
            if(memory[i]<min_value){
                min_value = memory[i];
            }
        }
        free(memory);
    } while (!cycle_stop);
    printf("[Reader %i] finish\n", params->thread_number);
    return NULL;
}

void fill_memory(int * memory_region){
    cycle_stop=0;
    //memory filling params
    //sem_init(&semaphore_urandom, 0, 1);
    int fd = open(RANDOM_FILE_NAME, O_RDONLY);
    pthread_t * memory_fillers = (pthread_t*) malloc(NUM_THREADS_MEMORY * sizeof(pthread_t));
    memory_fill_params * memory_data = (memory_fill_params *) malloc(NUM_THREADS_MEMORY * sizeof(memory_fill_params));
    int segment_size = (MEM_SIZE / 4 / NUM_THREADS_MEMORY) + 1;
    for(int i=0; i<NUM_THREADS_MEMORY; i++){
        memory_data[i].thread_num=i;
        memory_data[i].start_number= i * segment_size;
        memory_data[i].end_number = i != NUM_THREADS_MEMORY - 1 ? (i + 1) * segment_size : MEM_SIZE / 4;
        memory_data[i].memory_region = memory_region;
        memory_data[i].fd = fd;
        pthread_create(&(memory_fillers[i]), NULL, fill_memory_thread, &memory_data[i]);
    }
    //file writing params
    for(int i=0; i<=FULL_FILES_COUNT; i++){
        sem_init(&semaphore_file[i], 0, 1);
        sem_wait(&semaphore_file[i]); //блокируем все файлы, пока они не будут созданы
    }
    pthread_t * file_writer = (pthread_t *) malloc(sizeof(pthread_t));
    file_write_params * file_write_data = (file_write_params *) malloc(sizeof(file_write_params));
    file_write_data->memory_region = memory_region;
    pthread_create(file_writer, NULL, file_write_thread, (void *) file_write_data);

    //file reading params
    file_read_params * file_read_data = (file_read_params *) malloc(NUM_THREADS_FILE*sizeof(file_read_params));
    pthread_t * file_readers = (pthread_t*) malloc(NUM_THREADS_FILE * sizeof(pthread_t));
    for(int i=0; i<NUM_THREADS_FILE; i++){
        file_read_data[i].thread_number=i;
        file_read_data[i].file_number = i % FULL_FILES_COUNT;
        pthread_create(&(file_readers[i]), NULL, file_read_thread, &(file_read_data[i]));
    }

    sleep(1); //ждем, чтобы следующая надпись вывелась после старта всех потоков
    printf("Press [Enter] to interrupt infinite loop\n");
    getchar();
    printf("Waiting for all threads finish\n");
    cycle_stop=1;
    //wait for all threads
    for(int i=0; i<NUM_THREADS_MEMORY; i++){
        pthread_join(memory_fillers[i], NULL);
    }
    pthread_join(*file_writer, NULL);
    for(int i=0; i<NUM_THREADS_FILE; i++){
        pthread_join(file_readers[i], NULL);
    }
    //free memory
    free(memory_fillers);
    free(memory_data);
    free(file_writer);
    free(file_write_data);
    free(file_read_data);
    free(file_readers);
    //sem_destroy(&semaphore_urandom);
    for(int i=0; i<=FULL_FILES_COUNT; i++){
        sem_destroy(&semaphore_file[i]);
    }
    close(fd);
}


int main() {
    printf("Program is starting...\n");
    printf("Pause before memory allocation. Press [Enter] to continue\n");
    getchar();
    printf("Memory allocation...\n");
    int *memory_region = malloc(MEM_SIZE);
    //printf("%i", memory_region[10050000]);
    printf("Pause after memory allocation. Press [Enter] to continue\n");
    getchar();
    printf("Memory filling...\n");
    fill_memory(memory_region);
    //printf("%i", memory_region[10050000]);
    printf("Pause after memory filling. Press [Enter] to continue\n");
    getchar();
    printf("Minimal value is %i\n", min_value);
    printf("Memory releasing...\n");
    free(memory_region);
    return 0;
}
