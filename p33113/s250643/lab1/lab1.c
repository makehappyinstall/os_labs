#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <pthread.h>
#include <inttypes.h>
#include <sys/file.h>

#include "lab1.h"

#define MEMORYSIZE 221 * 1024 * 1024
#define MEMORYSTART 0xF8555978
#define THREADRANDOM 92
#define FILESSIZE 133 * 1024 * 1024
#define IOBLOCK 62
#define THREADREAD 65
#define J max
#define K flock

void* work_with_memory(){
    void* memory_pointer = (void*) MEMORYSTART;
    memory_pointer = mmap((void*) memory_pointer, MEMORYSIZE,
     PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    if (memory_pointer == MAP_FAILED){
        printf("Mapping Failed\n");
    }
    printf("Memory after allocation, Make a memory check Press Enter to continue.\n");
    getchar();
    write_to_memory(memory_pointer);
    printf("Memory filled with random values. Make a memory check and Press Enter.\n");
    getchar();

    munmap((void*) MEMORYSTART, MEMORYSIZE);
    printf("End of work with memory. Make a memory check and Press Enter.\n");
    getchar();
    return memory_pointer;
    // work_with_file(memory_pointer);
}

void write_to_memory(void* memory_pointer) {
    uint64_t total = MEMORYSIZE;
    uint64_t block = total / THREADRANDOM;
    pthread_t thread_id[THREADRANDOM];
    struct write_to_memory_piece piece[THREADRANDOM];
    for (uint8_t i = 0; i < THREADRANDOM; i++){
        piece[i].memory_pointer = (uint8_t*)memory_pointer + i * block;
        piece[i].size = block;

        pthread_create(thread_id + i, NULL, write_thread, piece + i);
    }

    for (uint8_t i = 0; i < THREADRANDOM; ++i) {
    	pthread_join(thread_id[i], NULL);
    }
}

void *write_thread(void *arg){
    struct write_to_memory_piece *piece = (struct write_to_memory_piece*) arg;
    FILE *urand = fopen("/dev/urandom", "rb");
    int result = fread(piece->memory_pointer, 1, piece->size, urand);
    if (!result) {
	    if (feof(urand)) {
	    printf("oaoaoaoa mmmmmm eof\n");
	    exit(-1);
	    }

	    if (ferror(urand)) {
	    perror("oaoaoaoa mmmmmm");
	    exit(-6);
	    }
    }
    fclose(urand);

    return 0;
}
/*
Записывает область памяти в файлы одинакового размера E (133) мегабайт
с использованием F=(блочного) обращения к диску.
Размер блока ввода-вывода G (62) байт. Преподаватель выдает в качестве
задания последовательность записи/чтения блоковпоследовательный
*/
void work_with_file(void* memory_pointer){
    printf("Start of work with files. Press Enter to continue\n");
    getchar();
    uint64_t files_count = ((MEMORYSIZE) / (FILESSIZE)) + 1;
    for (uint64_t i = 0; i < files_count; i++){
        char* filename = make_filename(i);
        FILE *f = fopen(filename, "wb");
        write_from_memory_to_file(f, memory_pointer);
    }
    printf("End of work with files. Press Enter to continue\n");
    getchar();

}

void write_from_memory_to_file(FILE *file, void* memory_pointer){
    for (uint64_t counter = 0; counter < FILESSIZE; counter += IOBLOCK){
        fwrite((uint8_t*)memory_pointer + counter, sizeof(uint8_t), IOBLOCK, file);
    }
    fclose(file);
}

/*
В отдельных I (65) потоках осуществлять чтение данных из файлов и подсчитывать
агрегированные характеристики данных - J=(максимальное).
*/
void work_with_threads(){
    printf("Start of work with threads. Press Enter to continue\n");
    getchar();
    uint64_t files_count = ((MEMORYSIZE) / (FILESSIZE)) + 1;
    for (uint64_t i = 0; i < files_count; i++){
        char* filename = make_filename(i);
        FILE *f = fopen(filename, "rb");
        max_in_file_reader_thread(f);
    }
}

void max_in_file_reader_thread(FILE *file){
    void* results[THREADREAD];
    int8_t max = INT8_MIN;
    uint64_t *results_ptr;
    pthread_t thread_id[THREADREAD];
    uint64_t size = (FILESSIZE) / THREADREAD;
    uint64_t offset = 0;
    for (uint64_t i = 0; i < THREADREAD - 1; i++){
        struct agr_state *state = malloc(sizeof(struct agr_state));
        state->fd = file;
        state->size = size;
        state->off = offset;
        pthread_create(&(thread_id[i]), NULL, aggreggate_state, state);
        offset += size;
    }
    int locked_file = fileno(file);
    flock(locked_file, LOCK_UN);
    for (uint8_t i = 0; i < THREADREAD - 1; i++){
            pthread_join(thread_id[i], results + i);
        }
    results_ptr = (uint64_t *) results;
    for (uint8_t i = 0; i < THREADREAD; i++){
        if (max < (int8_t) results_ptr[i]) max = results_ptr[i];
    }
    printf("files data max -> %d\n", max);
}

void * aggreggate_state(void* arg) {
    struct agr_state * state = arg;
    int64_t max = INT64_MIN;
    int8_t point = 0;
    int locked_file = fileno(state->fd);
    //block io
    flock(locked_file, LOCK_EX|LOCK_NB);
    fseek(state->fd, state->off, SEEK_SET);
    for (uint64_t i = 0; i < state->size; ++i) {
        point = fgetc(state->fd);
        if (max < point) {
            max = point;
        }
    }
    //unblock io
    flock(locked_file,LOCK_UN);
    return (void*)max;
}
/*
Чтение и запись данных в/из файла должна быть защищена примитивами
 синхронизации K=(flock).
*/

int main(int argc, char *argv[]){
    printf("Memory before allocation. Make a memory check Press Enter to continue.\n");
    getchar();
    void* pointer;
    pointer = work_with_memory();
    // while (1){
        work_with_file(pointer);
    // }
    work_with_threads();
    return 0;
}

char* make_filename(int name){
    switch (name) {
    case 0:
        return "./first.txt";
    case 1:
        return "./second.txt";
    case 2:
        return "./third.txt";
    case 3:
        return "./forth.txt";
    case 4:
        return "./fiveth.txt";
    default:
        return "./over.txt";
    }
}
