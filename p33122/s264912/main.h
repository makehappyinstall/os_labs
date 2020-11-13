#ifndef MAIN_H
#define MAIN_H
#include <stdint.h>
#include <stdio.h>
#define O_DIRECT 040000
void *write_thread(void *arg);
void init_sem();
void read_from_memory(int file, const unsigned char* memory_pointer);
void *read_and_sum(void* arg);
void close_signal(int32_t sig);
struct portion{
    void * memory_pointer;
    long size;
    long offset;
};
struct state{
    FILE *fd;
    long offset;
    long size;
};
enum errors {
    OK,
    FILE_ERR
};
#endif
