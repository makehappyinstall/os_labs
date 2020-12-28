#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <stdatomic.h>
//                                                                                             G = 133, I = 136,
const long long unsigned A = 154 * 1024 * 1024, B = 0xA71409AD, D = 72, E = 159 * 1024 * 1024, G = 133 * 1024, I = 136,
      number_of_files = (A % E != 0) ? (A / E) + 1 : A / E, number_of_threads_per_file = I / number_of_files;
FILE* urandom;
void* memptr;
volatile atomic_bool infinite_cycle = 0;
pthread_mutex_t the_real_flock_here;

struct thread_data {
    void* cursor;
    size_t chunk_size;
    uint file_number;
};

void* generate_data(void* provided_data) {
    struct thread_data* data = (struct thread_data*) provided_data;
    fread(data->cursor, data->chunk_size, 1, urandom);
}

void* write_to_file(void* provided_data) {
    struct thread_data* data = (struct thread_data*) provided_data;
    char path[] = "file0";
    path[4] += data->file_number;
    pthread_mutex_lock(&the_real_flock_here);
    uint fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
    posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
    while (flock(fd, LOCK_EX | LOCK_NB) == -1)
        usleep(1000);

    for (uint remains = data->chunk_size; remains > 0; remains -= G, data->cursor += G)
        write(fd, data->cursor, (G <= remains) ? G : remains);

    flock(fd, LOCK_UN);
    close(fd);
    pthread_mutex_unlock(&the_real_flock_here);
}

void* read_from_file(void* provided_data) {
    struct thread_data* data = (struct thread_data*) provided_data;
    long long unsigned int sum = 0;
    char path[] = "file0";
    path[4] += data->file_number;
    pthread_mutex_lock(&the_real_flock_here);
    FILE* file = fopen(path, "r+");
    while (flock(fileno(file), LOCK_EX | LOCK_NB) == -1)
        usleep(1000);
    //                    fread(&num, sizeof(uint), 1, file) == 1 -------------------------------------------
    for (uint i = 0, num; i < 10                                 ; i++, fread(&num, sizeof(uint), 1, file))
        sum += num;

    flock(fileno(file), LOCK_UN);
    fclose(file);
    pthread_mutex_unlock(&the_real_flock_here);
}

void* generate_data_multithreadingly(void* ignored) {
    pthread_t generation_threads[D];
    do {
        for (uint i = 0, chunk = A / D, covered = 0; i < D; i++, covered += chunk)
            pthread_create(&generation_threads[i], NULL, generate_data,
               &(struct thread_data){memptr + covered, chunk});
        for (uint i = 0; i < D; i++)
            pthread_join(generation_threads[i], NULL);
    } while (infinite_cycle);
}

void* write_to_files_multithreadingly(void* ignored) {
    pthread_t writing_threads[number_of_files];
    do {
        for (uint i = 0, remains = A, covered = 0; i < number_of_files; i++, remains -= E, covered += E)
            pthread_create(&writing_threads[i], NULL, write_to_file,
                &(struct thread_data){memptr + covered, (E <= remains) ? E : remains, i + 1});
        for (uint i = 0; i < number_of_files; i++)
            pthread_join(writing_threads[i], NULL);
    } while (infinite_cycle);
}

void* read_from_files_multithreadingly(void* ignored) {
    pthread_t reading_threads[I];
    do {
        for (uint i = 0; i < I; i++)
            pthread_create(&reading_threads[i], NULL, read_from_file,
                &(struct thread_data){NULL, 0, i / number_of_threads_per_file + 1});
        for (uint i = 0; i < I; i++)
            pthread_join(reading_threads[i], NULL);
    } while (infinite_cycle);
}

int main() {
    urandom = fopen("/dev/urandom\0", "r");
    printf("До аллокации");
    getchar();

    memptr = mmap((void*)B, A, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_FILE |MAP_PRIVATE, -1, 0);
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
