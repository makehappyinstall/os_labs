#define _GNU_SOURCE
#include <stdio.h>
#include <sched.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <stdatomic.h>

static const long long unsigned
//                                                                          G = 133-------, I = 136,
      A = 154 * 1024 * 1024, B = 0xA71409AD, D = 72, E = 159 * 1024 * 1024, G = 133 * 1024, I = 136,
      number_of_files = (A % E != 0) ? (A / E) + 1 : A / E, number_of_threads_per_file = I / number_of_files;
FILE* urandom;
int* memptr;
volatile atomic_bool infinite_cycle = 0;

struct thread_data {
    int* cursor;
    size_t chunk_size;
    uint file_number;
};

void* generate_data(void* provided_data) {
    struct thread_data* data = (struct thread_data*) provided_data;
    if (fread(data->cursor, data->chunk_size, 1, urandom) != 1)
        printf("Error in urandom occurred!\n");
    return NULL;
}

int write_to_file(void* provided_data) {
    struct thread_data* data = (struct thread_data*) provided_data;
    char path[] = "file0";
    path[4] += data->file_number;
    uint fd = open(path, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
    posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
    flock(fd, LOCK_EX);

    for (uint remains = data->chunk_size; remains > 0; remains -= G, data->cursor += G)
        write(fd, data->cursor, (G <= remains) ? G : remains);

    flock(fd, LOCK_UN);
    close(fd);
    return 0;
}

int read_from_file(void* provided_data) {
    struct thread_data* data = (struct thread_data*) provided_data;
    long long int sum = 0;
    int* buffer = malloc(1024 * sizeof(int));
    char path[] = "file0";
    path[4] += data->file_number;
    FILE* file = fopen(path, "r+");
    flock(fileno(file), LOCK_SH);

    for (;fread(buffer, 1024 * sizeof(int), 1, file) != 1;)
        for (int i = 0; i < 1024; i++)
            sum += buffer[i];
    printf("Sum of file is - %lld", sum);

    flock(fileno(file), LOCK_UN);
    fclose(file);
    free(buffer);
    return 0;
}

void* generate_data_multithreadingly(__attribute__((unused)) void* ignored) {
    pthread_t* generation_threads = malloc(D * sizeof(pthread_t));
    struct thread_data data;

    do {
        for (uint i = 0, chunk = A / D, covered = 0; i < D; i++, covered += chunk) {
            data = (struct thread_data){memptr + covered, chunk};
            pthread_create(&generation_threads[i], NULL, generate_data, &data);
        }
        for (uint i = 0; i < D; i++)
            pthread_join(generation_threads[i], NULL);
    } while (infinite_cycle);

    free(generation_threads);
    return NULL;
}

void* write_to_files_multithreadingly(__attribute__((unused)) void* ignored) {
    pid_t* writing_threads = malloc(number_of_files * sizeof(pid_t));
    int* ptr = malloc(number_of_files * 1024 * 1024);
    struct thread_data data;

    do {
        for (uint i = 0, remains = A, covered = 0; i < number_of_files; i++, remains -= E, covered += E) {
            data = (struct thread_data) {memptr + covered, (E <= remains) ? E : remains, i + 1};
            writing_threads[i] = clone(write_to_file, ptr + (1024 * i) + (1024 * 1024), SIGCHLD, &data);
        }
        for (uint i = 0; i < number_of_files; i++)
            while (kill(writing_threads[i], 0) != 0)
                usleep(1000);
    } while (infinite_cycle);

    free(ptr);
    free(writing_threads);
    return NULL;
}

void* read_from_files_multithreadingly(__attribute__((unused)) void* ignored) {
    pid_t* reading_threads = malloc(I * sizeof(pid_t));
    int* ptr = malloc(I * 1024 * 1024);
    struct thread_data data;

    do {
        for (uint i = 0; i < I; i++) {
            data = (struct thread_data) {NULL, 0, i / number_of_threads_per_file + 1};
            reading_threads[i] = clone(read_from_file, ptr + (1024 * i) + (1024 * 1024), SIGCHLD, &data);
        }
        for (uint i = 0; i < I; i++)
            while (kill(reading_threads[i], 0) != 0)
                usleep(1000);
    } while (infinite_cycle);

    free(ptr);
    free(reading_threads);
    return NULL;
}

int main() {
    urandom = fopen("/dev/urandom\0", "r");
    printf("До аллокации");
    getchar();

    memptr = mmap((void *) B, A, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_FILE | MAP_PRIVATE, -1, 0);
    printf("После аллокации");
    getchar();

    generate_data_multithreadingly(NULL);
    printf("После заполнения участка данными");
    getchar();

    pthread_t cycle_threads[3];
    infinite_cycle = 1;
    pthread_create(&cycle_threads[0], NULL, generate_data_multithreadingly, NULL);
    pthread_create(&cycle_threads[1], NULL, write_to_files_multithreadingly, NULL);
    pthread_create(&cycle_threads[2], NULL, read_from_files_multithreadingly, NULL);
    printf("Программа работает в режиме бесконечного цикла");
    getchar();

    infinite_cycle = 0;
    pthread_join(cycle_threads[0], NULL);
    pthread_join(cycle_threads[1], NULL);
    pthread_join(cycle_threads[2], NULL);
    munmap(memptr, A);
    printf("После деаллокации");
    getchar();
    fclose(urandom);
}
