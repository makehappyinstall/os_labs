//
// Created by sdfedorov on 24/11/2020.
//
#include <stdio.h>
#include <limits.h>

#include "memory_ops.h"
#include "file_ops.h"
#include "constants.h"
#include "util.h"

int main(){
    wait_for_input("ALLOCATE MEM");
    void* mem_region = allocate_memory((void*) B, A * megabyte);
    wait_for_input("FILL MEM");
    fill_the_memory(mem_region, A * megabyte, "/dev/urandom", D);
    wait_for_input("WRITE MEM TO FILES");
    write_rnd_mem_to_files(mem_region, A * megabyte, E * megabyte, G);
    wait_for_input("READ AND AGGREGATE FROM FILES");
    long result = aggregate_value_from_files(A * megabyte, E * megabyte, I, LONG_MAX, &min);
    printf("RESULTED VALUE: %ld\n", result);

    puts("END");
}
