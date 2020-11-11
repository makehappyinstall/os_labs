#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <stdint.h>

struct portion {
	void* mem_pointer;
	uint64_t size;
	uint64_t start;
};

struct thread_args {
	FILE* file_d;
	uint64_t size;
	uint64_t offset;

};

void thread_init_for_write(void* memory_pointer);
void* write_to_memory(void* struct_address);
void read_from_memory_to_file(FILE* file, void* memory_pointer);
uint64_t thread_init_for_read(void);
void* func_sum(void* th_args);
void i_signal(int32_t signal);

#endif