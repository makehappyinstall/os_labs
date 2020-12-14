//
// Created by sdfedorov on 24/11/2020.
//

#include <stddef.h>

#ifndef OPERATING_SYSTEMS_ITMO_2020_FILE_OPS_H
#define OPERATING_SYSTEMS_ITMO_2020_FILE_OPS_H

#define POSIX_FADVISE

#define DEBUG

void write_rnd_mem_to_files(void* addr, size_t mem_size, size_t file_size_limit, size_t block_size);

long aggregate_value_from_files(size_t mem_size, size_t file_size, int thread_count, long fold_start, long(agg_func)(long, long));

#endif //OPERATING_SYSTEMS_ITMO_2020_FILE_OPS_H
