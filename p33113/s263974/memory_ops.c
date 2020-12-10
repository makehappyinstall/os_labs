//
// Created by sdfedorov on 24/11/2020.
//

#include <sys/mman.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include "memory_ops.h"

struct to_fill_region {
    void *ptr;
    size_t size, offset;
    const char* read_from;
};

static void* filling_thread(void *arg);

void* allocate_memory(void* addr, size_t size) {
    return mmap(addr, size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
}

void fill_the_memory(void* addr, size_t size, const char* read_from, int thread_count) {
    pthread_t thread_ids[thread_count];
    struct to_fill_region regions[thread_count];

    size_t size_per_thread = size / thread_count;
    size_t memory_remains = size;

    int i;
    struct to_fill_region* cur_region;
    for (i = 0, cur_region = regions; i < thread_count; i++, cur_region = regions + i) {
        if ((memory_remains -= size_per_thread) < size_per_thread) {
            size_per_thread += memory_remains;
            memory_remains = 0;
        }

        cur_region->ptr = addr;
        cur_region->size = size_per_thread;
        cur_region->offset = memory_remains;
        cur_region->read_from = read_from;

        pthread_create(thread_ids + i, NULL, filling_thread, cur_region);
    }

    for (i = 0; i < thread_count; i++) {
        pthread_join(*(thread_ids + i), NULL);
    }
}

static void* filling_thread(void *arg){
    struct to_fill_region* region = (struct to_fill_region*) arg;
    int file_d = open(region->read_from, O_RDONLY);
    if (file_d == -1) {
        perror("Error on opening the file");
        exit(errno);
    } else {
        void* write_to;
        size_t read_bytes;
        size_t successfully_read = 0;
        while(successfully_read < region->size) {
            write_to = ((char*) region->ptr + region->offset);
            read_bytes = read(file_d, write_to, region->size - successfully_read);
            if (read_bytes == -1) {
                perror("Error on reading the file");
                exit(errno);
            } else {
                successfully_read += read_bytes;
                printf("FILLED %p with %lu bytes from %s (%lu remains)\n", write_to, read_bytes, region->read_from, region->size - successfully_read);
            }
        }
    }
    return 0;
}
