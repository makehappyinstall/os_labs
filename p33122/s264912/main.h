#ifndef MAIN_H
#define MAIN_H
#include <stdio.h>
void *write_thread(void *arg);
void init_sem();
void read_from_memory(int file, const char* memory_pointer);
const char* file_naming(int file_num);
int create_wronly_file(int file_num);
void *read_and_sum(void* arg);
void close_signal(int32_t sig);
#endif
