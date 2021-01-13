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

#define A 193
#define D 119
#define E 150
#define G 19
#define I 57

int r_cond = 1;
int w_cond = 1;
int number_of_files;
unsigned char *ptr;

typedef struct{
    char * start;
    size_t count;
    FILE * urandom;
} thread_args;

typedef struct {
    pthread_cond_t *cond_vars;
    pthread_mutex_t *mutexes;
} lock_tool;

void* write_random_numbers(void * in_data){
    thread_args *data = (thread_args*) in_data;
    fread((void *) data->start, 1, data->count, data->urandom);
    return NULL;
}

void fill_memory(char * memory_region){
    char * thread_data_start = memory_region;
    FILE * urandom = fopen("/dev/urandom", "r");
    size_t part =  A * 1024 * 1024 / D;
    pthread_t threads[D];
    thread_args threads_data[D];

    for (int i = 0; i < D; i++) {
        if(i==D-1){
            threads_data[i].count = part + (A * 1024 * 1024 % D);
        }else{
            threads_data[i].count = part;
        }
        threads_data[i].urandom = urandom;
        threads_data[i].start = thread_data_start;
        pthread_create(&(threads[i]), NULL, write_random_numbers, &(threads_data));
        thread_data_start+=part;
    }

    for (int i = 0; i < D; i++) pthread_join(threads[i], NULL);
    
    fclose(urandom);
}

void* thread_func_write_files(void* args ){
    lock_tool *lt = (lock_tool*)args;
    int file_handle;
    int block_size;
    int number_of_blocks;
    unsigned char *address;
    char filename[8];

    struct stat buff;
    do{
        for(int i = 0; i < number_of_files; i++){
            pthread_mutex_lock(&lt->mutexes[i]);
            sprintf(filename, "file_%d", i);
            file_handle = open(filename, O_CREAT | O_WRONLY | O_DIRECT | O_TRUNC, __S_IREAD | __S_IWRITE);
            if(file_handle == -1){
                printf("Smth went wrong with file reading.\n");
                break;
            }

            stat(filename, &buff);
            block_size = (int) buff.st_blksize;
            number_of_blocks = E * 1024 * 1024 / block_size;
            char *block = (char *) malloc(2 * block_size - 1);
            char *wblock = (char *) (((uintptr_t) block + block_size - 1) & ~((uintptr_t) block_size - 1));
            
            for(int j = 0; j < number_of_blocks; j++){    
                address = ptr + i * (E * 1024 * 1024) + block_size * j;
            
                memcpy(wblock, address, block_size);
                if(pwrite(file_handle, wblock, block_size, j*block_size) == -1){
                    
                    close(file_handle);
                    free(block);
                }
            }
            close(file_handle);
            free(block);
            pthread_cond_broadcast(&lt->cond_vars[i]);
            pthread_mutex_unlock(&lt->mutexes[i]);
            printf("End write\n");
        }
    }while(w_cond);
    return NULL;
}

void* thread_func_read_files(void* args){
    lock_tool *lt =(lock_tool*) args;
    char filename[8];
    long long number_of_blocks = E * 1024 * 1024 / G;
    do{
        for(int i = 0; i < number_of_files; i++){    

            pthread_mutex_lock(&lt->mutexes[i]);
            printf("Waiting cond var for file #%d\n", i);
            pthread_cond_wait(&lt->cond_vars[i], &lt->mutexes[i]);
            
            long long sum = 0;
            sprintf(filename, "file_%d", i);
            FILE *file = fopen(filename, "r");
            char buffer[G];

            for(long long j = 0; j < number_of_blocks; j++){
                
                if (fread(&buffer, 1, G, file) != G){
                    continue;
                }
                for(int k = 0; k < G; ++k){
                    int num = *((int *) &buffer[k]);
                    sum += num;
                }
            }
            printf("File #%d has sum=%lld\n", i, sum);
            fclose(file);
        
            pthread_mutex_unlock(&lt->mutexes[i]);
        }
    }while(r_cond);
    return NULL;
}

void fill_files(){
    number_of_files = A / E;

    lock_tool writer_tools;
    lock_tool reader_tools;

    pthread_cond_t *cond_vars = malloc(sizeof(pthread_cond_t) * number_of_files);
    pthread_mutex_t *mutexes = malloc(sizeof(pthread_mutex_t) * number_of_files);
    for(int i = 0; i < number_of_files; i++){
        pthread_mutex_init(&mutexes[i], NULL);
        pthread_cond_init(&cond_vars[i], NULL);

        writer_tools.cond_vars = cond_vars;
        writer_tools.mutexes = mutexes;
        
        reader_tools.cond_vars = cond_vars;
        reader_tools.mutexes = mutexes;
    }
    
    pthread_t w_thread;
    pthread_create(&w_thread, NULL, thread_func_write_files, &writer_tools);
    
    
    pthread_t r_threads[I];
    for(int i = 0; i < I; i++){
        pthread_create(&r_threads[i], NULL, thread_func_read_files, &reader_tools);
    }

    printf("Press key to stop\n");
    getchar();

    r_cond = 0;
    for(int i = 0; i < I; i++) pthread_join(r_threads[i], NULL);

    w_cond = 0;
    pthread_join(w_thread, NULL);

    free(cond_vars);
    free(mutexes);
}
    
int main (void)
{
    printf("Start create and fill memory area\n");
    long long int memory_size = A * 1024 * 1024;

    printf("Снять до аллокации\n");
    getchar();

    char *memory_region = malloc(memory_size);
    ptr = (unsigned char *)memory_region;

    printf("Снять после аллокации\n");
    getchar();

    fill_memory(memory_region);
    
    printf("Start create and fill files\n");
    fill_files();

    printf("Снять после заполнения участка данными\n");
    getchar();
    
    free(memory_region);

    printf("Снять после деаллокации\n");
    getchar();
}