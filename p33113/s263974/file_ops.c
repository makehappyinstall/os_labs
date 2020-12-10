//
// Created by sdfedorov on 24/11/2020.
//

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

#include "file_ops.h"

#define ceil(a, b) ((a % b) != 0 ? a / b + 1 : a / b)
#define rnd_offset(upper_bound) ((size_t) random() % upper_bound)

struct aggregating_thread_state{
    int fd;
    pthread_cond_t* file_cv;
    pthread_mutex_t* file_mutex;
    long size;
    long* thread_result;
    long(*agg_func)(long, long);
    long fold_start;
};

static const char* generate_file_name(int file_num);
static int create_file(const char* file_name);
static void write_rnd_mem_to_file(int fd, void* mem_ptr, size_t mem_size, size_t block_size, size_t file_size);
static void* aggregating_thread(void* arg);

void write_rnd_mem_to_files(void* addr, size_t mem_size, size_t file_size_limit, size_t block_size) {
    int file_count = ceil(mem_size, file_size_limit);

    int i;
    for (i = 0; i < file_count; i++) {
        const char* new_file_name = generate_file_name(i + 1);
        int new_file = create_file(new_file_name);
        printf("Writing to %s\n", new_file_name);
        write_rnd_mem_to_file(new_file, addr, mem_size, block_size, file_size_limit);
        close(new_file);
    }
}

long aggregate_value_from_files(size_t mem_size, size_t file_size, int thread_count, long fold_start, long(*agg_func)(long, long)) {
    int file_count = ceil(mem_size, file_size);

    int threads_per_file = thread_count / file_count;
    long size_per_thread = (long long) file_size / threads_per_file;
    long remainder_size =  (long long) file_size % threads_per_file;

    pthread_t thread_ids[thread_count];
    struct aggregating_thread_state threads[thread_count];
    long results[thread_count];

    int file_iter, thread_iter;
    int files[file_count];
    pthread_cond_t files_cv[file_count];
    pthread_mutex_t files_mutex[file_count];

    for (file_iter = 0; file_iter < file_count; file_iter++) {
        const char* file_name = generate_file_name(file_iter+1);
        files[file_iter] = create_file(file_name);
        files_cv[file_iter] = (pthread_cond_t) PTHREAD_COND_INITIALIZER;
        files_mutex[file_iter] = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
        printf("Aggregating from %s\n", file_name);
    }

    for (file_iter=0, thread_iter=0; thread_iter < thread_count;) {
        struct aggregating_thread_state* cur_thread = threads + thread_iter;
        cur_thread->fd = files[file_iter];
        cur_thread->file_cv = files_cv + file_iter;
        cur_thread->file_mutex = files_mutex + file_iter;
        cur_thread->size = ((thread_iter + 1 == thread_count) && remainder_size != 0) ? remainder_size : size_per_thread;
        cur_thread->thread_result = results + thread_iter;
        cur_thread->agg_func = agg_func;
        cur_thread->fold_start = fold_start;

        pthread_create(thread_ids + thread_iter, NULL, aggregating_thread, cur_thread);

        thread_iter++;
        if (file_iter + 1 >= file_count) {
            file_iter = 0;
        } else file_iter++;
    }

    for (file_iter = 0; file_iter < file_count; file_iter++)
        pthread_cond_signal(files_cv + file_iter);

    long global_fold_val = fold_start;
    for(thread_iter = 0; thread_iter < thread_count; thread_iter++){
        pthread_join(thread_ids[thread_iter], NULL);
        global_fold_val = agg_func(global_fold_val, *(results + thread_iter));
    }

    return global_fold_val;
}

static void write_rnd_mem_to_file(int fd, void* mem_ptr, size_t mem_size, size_t block_size, size_t file_size) {
    unsigned long iter = 0;
    long long remains = file_size;
    puts("Started writing to file");
    while (remains > 0) {
        size_t rnd_offset = rnd_offset(mem_size - block_size - 1);
        void* rnd_ptr = (char*) mem_ptr + rnd_offset;

        if (iter % 100000 == 0) {
            printf("Iter %lu: writing %lu bytes from (%p->%p<-%p) to %d (%lld left to write)\n",
                   iter, block_size, mem_ptr, rnd_ptr, (char*) mem_ptr + mem_size, fd, remains);
        }

        write(fd, rnd_ptr, block_size);

        remains -= (long long) block_size;
        iter++;
    }
    puts("Finished writing to file");
}

static void* aggregating_thread(void* arg) {
    struct aggregating_thread_state* cur_thread = (struct aggregating_thread_state*) arg;
    char read_chars[cur_thread->size];

    pthread_mutex_lock(cur_thread->file_mutex);
    pthread_cond_wait(cur_thread->file_cv, cur_thread->file_mutex);
    size_t read_bytes = read(cur_thread->fd, read_chars, cur_thread->size);

    switch (read_bytes) {
        case 0:
            printf("Finished reading file %d\n", cur_thread->fd);
            break;
        case -1:
            perror("Error reading the file");
            exit(errno);
        default:
            printf("Read %lu bytes from %d\n", read_bytes, cur_thread->fd);
    }

    pthread_cond_signal(cur_thread->file_cv);
    pthread_mutex_unlock(cur_thread->file_mutex);

    int i;
    long local_fold_val = cur_thread->fold_start;
    for (i = 0; i < cur_thread->size; i++)
        local_fold_val = cur_thread->agg_func(local_fold_val, (long) read_chars[i]);

    printf("Thread computed aggregated value: %ld\n", local_fold_val);

    *cur_thread->thread_result = local_fold_val;
    return 0;
}

static const char* generate_file_name(int file_num) {
    int size = sizeof(char) * 20;
    char* name = malloc(size);
    snprintf(name, size, "./file%d.bin", file_num);
    return name;
}

static int create_file(const char* file_name) {
    int file = open(file_name, O_RDWR | O_CREAT, (mode_t) 0600);
#ifdef POSIX_FADVISE
    posix_fadvise(file, 0, 0, POSIX_FADV_DONTNEED);
    printf("Advised system to use NOCACHE with %s\n", file_name);
#endif
    if (file == -1){
        perror("Error on creating the file");
        exit(errno);
    }

    return file;
}
