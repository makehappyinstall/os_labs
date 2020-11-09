#ifndef LAB1_H
#define LAB2_H
#include <stdint.h>
void write_to_memory(void* memory_pointer);
void* work_with_memory();
void *write_thread(void *arg);
void work_with_file(void* memory_pointer);
void write_from_memory_to_file(FILE *file, void* memory_pointer);
char* make_filename(int name);
void work_with_threads();
void max_in_file_reader_thread(FILE *file);
void *aggreggate_state(void* arg);

struct write_to_memory_piece {
    void* memory_pointer;
    uint64_t size;
};

struct agr_state{
    FILE *fd;
    uint64_t off;
    uint64_t size;
};
#endif
