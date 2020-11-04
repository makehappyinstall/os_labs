#include <stdio.h>
#include<sys/mman.h>
#include <pthread.h>
#include <stdlib.h>

#define A 248
#define B 0x936F88A5
#define D 131
#define E 170
#define G 112
#define I 29

typedef struct{
    unsigned char * start;
    size_t count;
    FILE * urandom;
} wrn_args;

typedef struct {
    int file_number;
    pthread_mutex_t * mutex;
    pthread_cond_t * cv;
} ras_args;

typedef struct {
    unsigned char * src;
    unsigned int file_number;
    pthread_mutex_t * mutex;
    pthread_cond_t * cv;
} wif_args;

void* write_random_numbers(void * wrn_data){
    wrn_args *data = (wrn_args*) wrn_data;
    for (size_t i = 0; i < data->count; ) {
        i += fread(data->start + i, 1, data->count - i, data->urandom);
    }
    return NULL;
}

void generate_in_mem(unsigned char *src){
    FILE * urandom = fopen("/dev/urandom", "rb");
    size_t wrn_part =  A * 1024 * 1024 / D;
    unsigned char * start = src;
    pthread_t wrn_threads[D];
    for (int i = 0; i<D-1; i++){
        wrn_args wrn_data = {start, wrn_part, urandom};
        pthread_create(&(wrn_threads[i]), NULL, write_random_numbers, &wrn_data);
        start+=wrn_part;
    }

    wrn_args wrn_data = {start, wrn_part + A * 1024 * 1024 % D, urandom};
    pthread_create(&(wrn_threads[D-1]), NULL, write_random_numbers, &wrn_data);
    for(int i = 0; i < D; i++)
        pthread_join(wrn_threads[i], NULL);

    fclose(urandom);
}

_Noreturn void* endless_g_i_m(void * endless_gim_args){
    unsigned char * src = endless_gim_args;
    while(1) generate_in_mem(src);
}

void write_in_file(unsigned char *src,
                   unsigned int file_number,
                   pthread_mutex_t * mutex,
                   pthread_cond_t * cv){

    char result_name[13];
    snprintf(result_name,13, "lab_os_%d.bin", file_number);
    pthread_mutex_lock(mutex);
    FILE * file = fopen(result_name, "wb");
    size_t file_size = E*1024*1024;
    for (unsigned long j = 0; j < file_size; ){
        unsigned int block_size = file_size - j < G ? file_size - j : G;
        j += fwrite( src + j, 1, block_size, file);
    }
    fclose(file);
    printf("%d data_generated\n", file_number);
    pthread_cond_broadcast(cv);
    pthread_mutex_unlock(mutex);
}

_Noreturn void* thread_wif(void * thread_wif_args){
    wif_args * args = thread_wif_args;
    while(1) write_in_file(args->src, args->file_number, args->mutex, args->cv);
}

int convert_char_buf_2_int(unsigned char buf[]){
    int sum = 0;
    for (int i = 0; i < G / sizeof (int); i+=sizeof (int)){
        int num = 0;
        for (int j = 0; j < sizeof (int); j++){
            num = (num<<8) + buf[i+j];
        }
        sum += num;
    }
    return sum;
}

_Noreturn void* read_and_sum(void * ras_data){
    ras_args * args = ras_data;
    unsigned int f_n = args->file_number;
    char filename[13];
    snprintf(filename,  13, "lab_os_%d.bin", f_n);
    while(1){
        long long sum = 0;
        pthread_mutex_lock(args->mutex);
        printf("%d wait on cond\n", f_n);
        pthread_cond_wait(args->cv, args->mutex);
        printf("%d waited\n", f_n);
        FILE *file = fopen(filename, "rb");
        unsigned char buf[G];
        for (unsigned int i = 0; i < 2*E*1024*1024/G; i++){
            if (fread(&buf, 1, G, file) != G) continue;
            else sum+=convert_char_buf_2_int(buf);
        }
        printf("%d: %lld\n", f_n, sum);
        fclose(file);
        pthread_mutex_unlock(args->mutex);
    }
}

unsigned char* allocate_memory(){
    return mmap((void *)B, A*1024*1024, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

int main() {
    unsigned char *src;
    printf("Before allocation");
    getchar();
    src = allocate_memory();
    printf("After allocation");
    getchar();
    generate_in_mem(src);
    printf("After filling");
    getchar();
    munmap(src, A*1024*1024);
    printf("After deallocation");
    getchar();

    const unsigned int file_number = A / E + (A % E > 0 ? 1 : 0);
    src = allocate_memory();

    pthread_mutex_t mutexes[file_number];
    pthread_cond_t cvs[file_number];
    for (int i = 0; i < file_number; i++){
        pthread_mutex_init(&(mutexes[i]), NULL);
        pthread_cond_init(&(cvs[i]), NULL);
    }

    pthread_t ras_threads[I];
    ras_args ras_data[file_number];

    for (int i = 0; i < I; i++){
        int file_count = i / (I / file_number);
        if (file_count == file_number) --file_count;
        ras_data[file_count].file_number = file_count;
        ras_data[file_count].mutex = &(mutexes[file_count]);
        ras_data[file_count].cv = &(cvs[file_count]);
        pthread_create(&ras_threads[i], NULL, read_and_sum, &(ras_data[file_count]));
    }

    pthread_t gim;
    pthread_create(&gim, NULL, endless_g_i_m, src);

    pthread_t file_threads[file_number];
    wif_args wif_data[file_number];
    for(unsigned int i = 0; i < file_number; i++){
        wif_data[i].src = src;
        wif_data[i].file_number = i;
        wif_data[i].mutex = &(mutexes[i]);
        wif_data[i].cv = &(cvs[i]);
        pthread_create(&file_threads[i], NULL, thread_wif, &(wif_data[i]));
    }

    for (int i = 0; i < I; i++)
        pthread_join( ras_threads[i], NULL);

    pthread_join(gim, NULL);

    for (int i = 0; i < file_number; i++){
        pthread_join(file_threads[i], NULL);
    }

    return 0;
}
