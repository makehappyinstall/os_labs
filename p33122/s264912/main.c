#include <stdlib.h>
#include <pthread.h>
#include <sys/mman.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include <math.h>
#include "./constants.h"
#include "./main.h"

static sem_t fileSync; // статическая память, заранее до начла исполнения, и потом так и остается

struct portion{
    void * memory_pointer;
    long size;
    long offset;
};
struct state{
    FILE *fd;
    long offset;
    long size;
    unsigned long long* resulted_sum;
};
enum errors {
    OK,
    FILE_ERR
};

int min(int a, int b){
    return (a < b) ? a : b;
}

int main(int args, char *argv[]){
    // до аллокации
    puts("Before allocate");
    getchar();
    //void* memory = (void *) 0x5845DAC9;

    // mmap(addr, lenght, prot, flags, fd, offset)
    // addr = memory
    // length = 46 mb = 46*1024*1024 b
    // разместили память рядом с выбранной
    // prot = разрешение даем на чтение и запись
    // flags = не используем файл и совместо с другими процессами
    // fd = раз файла нет то -1
    // offset = смещение 0
    void* memory = mmap((void *)B, A *1024*1024,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0); // динамический,
    // на куче выделяется, можно напрямую взаимодействовать, нужно освободать
    assert(memory != MAP_FAILED);

    //после аллокации
    puts("After allocate");
    getchar();
    signal(SIGINT, close_signal);

    while(1){

        long total_size = A * 1024* 1024;
        //long remaining_size = total_size;
        long size_for_one_thread = 3014656; //total_size / 16
        pthread_t thread_id[D];
        struct portion *portions = malloc(sizeof(struct portion) * D);//будет указатель на адрес первого байта структуры
        for (char i = 0; i < D; i++){
            struct portion *block = portions + i;//будет указатель на адрес первого байта структуры
            if((total_size -= size_for_one_thread) < size_for_one_thread){
                size_for_one_thread += total_size;
                total_size = 0;
            }
            block->memory_pointer = memory;
            block->size = size_for_one_thread;
            block->offset = total_size;

            //pthread_create(threadid, attr, start_routing, arg);
            pthread_create(thread_id + i, NULL, write_thread, block);
        }
        //pthread_join(thread, retval)
        // тут нужен чтоб дождаться завершения потока
        for (size_t i = 0; i < D; i++) {
            pthread_join(thread_id[i], NULL);
        }
        free(portions);

        int file_count = (int) ceil((double) A / (double) E);
        int memory_left = A * 1024 * 1024;
        int file_size = E * 1024 * 1024;

        for (int i = 0; i < file_count; i++){
            int new_file = create_wronly_file(i+1);
            memory_left -= file_size;
            read_from_memory(new_file, (char*) memory + file_size * i + min(0, memory_left));
            close(new_file);
        }

        init_sem();

        pthread_t threads_id[I];
        int threads_per_file = I / file_count;
        long size_per_thread = file_size / threads_per_file;
        long remainder_size = file_size % threads_per_file;
        long offset = 0;

        struct state* states = malloc(sizeof(struct state) * I);
        unsigned long long* resulted_sums = malloc(sizeof(unsigned long long) * I);

        FILE* files[file_count];
        for(int i = 0; i < file_count; i++){
            const char* naming = file_naming(i+1);
            files[i] = fopen(naming, "rb");
            assert(files[i] != NULL);
            printf("Reading RND NUM: %s\n", naming);
        }

        int file_iter, thread_iter;
        for (file_iter=0, thread_iter=0; thread_iter < I;) {
            struct state* tmp_state = states + thread_iter;
            tmp_state->fd = files[file_iter];
            tmp_state->size = ((thread_iter + 1 == I) && remainder_size != 0) ? remainder_size : size_per_thread;
            tmp_state->offset = offset;
            tmp_state->resulted_sum = resulted_sums + thread_iter;

            pthread_create(threads_id + thread_iter, NULL, read_and_sum, tmp_state);

            // INCREMENT
            thread_iter++;
            if (file_iter + 1 >= file_count) {
                offset += size_per_thread;
                file_iter = 0;
            } else file_iter++;
        }


        unsigned long long int sum = 0;

        for(size_t i = 0; i < I; i++){
            void* tmp;
            pthread_join(threads_id[i], &(tmp));
            sum += *(resulted_sums + i);
        }

        printf("RND NUM SUM: %lld\n",sum);
        free(states);
        free(resulted_sums);

        //после аллокации
        puts("After writing");
        getchar();
    }
}

const char* file_naming(int file_num) {
    int naming_size = sizeof(char) * 20;

    char* naming = malloc(naming_size);
    snprintf(naming, naming_size, "./file%d.bin", file_num);

    return naming;
}

int create_wronly_file(int file_num) {
    const char* naming = file_naming(file_num);
    printf("Writing RND NUM: %s\n", naming);

    int file = open(naming, O_RDWR | O_CREAT, (mode_t) 0600); //  666–0077=0600, то есть rw-------
    posix_fadvise(file, 0, 0, POSIX_FADV_DONTNEED); // для nocache

    if (file == -1){
        perror("Error with file");
        exit(FILE_ERR);
    }

    return file;
}

void init_sem(){
    sem_init(&fileSync, 0, 1);
}

void* write_thread(void *arg){
    struct portion *block = (struct portion*) arg;
    FILE *random = fopen("/dev/urandom", "rb");
    fread((char *)block->memory_pointer + block->offset, 1, block->size, random);
    return NULL;
}

void read_from_memory(int file, const char* memory_pointer){
    int remains = E * 1024 * 1024;
    size_t answer;

    while (remains > 0) {
        answer = write(file, memory_pointer, min(sizeof(char)*G, remains));
        if (answer == -1){
            perror("Cant write to file");
            return;
        }

        remains -= G;
        memory_pointer += G;
    }
}

void* read_and_sum(void* arg){
    struct state* state = (struct state*) arg;
    unsigned long long sum = 0; // автоматический тип память на стеке, улетаюююююют после функции
    long point;
    sem_wait(&fileSync);
    fseek(state->fd, state->offset, SEEK_SET); //переставляет указатель в файле
    for (long counter = 0; counter != state->size; counter++){
        point = fgetc(state->fd);//получает один char из файла
        sum += point;
    }
    *state->resulted_sum = sum;
    sem_post(&fileSync);

    return NULL;
}

void close_signal(int32_t sig){
    puts("After deallocate");
    getchar();
    exit(0);
}
