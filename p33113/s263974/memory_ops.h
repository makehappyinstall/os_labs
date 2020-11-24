//
// Created by sdfedorov on 24/11/2020.
//

#include <stddef.h>

#ifndef OPERATING_SYSTEMS_ITMO_2020_MEMORY_OPS_H
#define OPERATING_SYSTEMS_ITMO_2020_MEMORY_OPS_H

void* allocate_memory(void* addr, size_t size);
void fill_the_memory(void* addr, size_t size, const char* read_from, int thread_count);

#endif //OPERATING_SYSTEMS_ITMO_2020_MEMORY_OPS_H
