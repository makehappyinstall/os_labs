#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <stdatomic.h>
#include <linux/futex.h>
#include <syscall.h>
#include <string.h>

#define print_thread_activity 0
#define megabyte_size 1024*1024
#define A 300
#define D 73
#define E 150
#define G 136
#define I 147
#define observe_block 0
#define print_sum 1

int infinity_loop = 1;

typedef struct {
    int thread_number;
    int ints_per_thread;
    int *start;
    FILE *file;
} thread_generator_data;

typedef struct {
    int ints_per_file;
    int files;
    int *start;
    int *end;
    int *futexes;
} thread_writer_data;

typedef struct {
    int thread_number;
    int file_number;
    int *futexes;
} thread_reader_data;

static int
futex(int *uaddr, int futex_op, int val,
      const struct timespec *timeout, int *uaddr2, int val3)
{
    return syscall(SYS_futex, uaddr, futex_op, val,
                   timeout, uaddr2, val3);
}
static void
fwait(int *futexp)
{
    while (1) {
        const int one = 1;
        if (atomic_compare_exchange_strong(futexp, &one, 0))
            break;
        futex(futexp, FUTEX_WAIT, 0, NULL, NULL, 0);
    }
}

static void
fpost(int *futexp)
{
    const int zero = 0;
    if (atomic_compare_exchange_strong(futexp, &zero, 1)) {
        futex(futexp, FUTEX_WAKE, 1, NULL, NULL, 0);
    }
}

int generators_filled = 0;

int read_int_from_file(FILE *file) {
    int i = 0;
    fread(&i, 4, 1, file);
    return i;
}

char* seq_read(int fd, int file_size) {
    char *buffer = (char*) malloc(file_size);
    const int blocks = file_size / G;
    const int last_block_size = file_size % G;
    char* buf_ptr;
    for (int i = 0; i < blocks; ++i) {
        buf_ptr = buffer + G*i;
        pread(fd, buf_ptr, G, G*i);
    }
    if (last_block_size > 0) {
        buf_ptr = buffer + G*blocks;
        pread(fd, buf_ptr, last_block_size, G*blocks);
    }
    return buffer;
}

void *fill_with_random(void *thread_data) {
    thread_generator_data *data = (thread_generator_data *) thread_data;
    if (print_thread_activity) {
        printf("[GENERATOR-%d] started...\n", data->thread_number);
    }
    int first_run = 1;
    do {
        for (int i = 0; i < data->ints_per_thread; i++) {
            data->start[i] = read_int_from_file(data->file);
        }
        if (first_run) {
            atomic_fetch_add(&generators_filled, 1);
            first_run = 0;
            if (generators_filled == D) {
                printf("После заполнения данными\n");
            }
        }
    } while (infinity_loop);
    if (print_thread_activity) {
        printf("[GENERATOR-%d] finished...\n", data->thread_number);
    }
    return NULL;
}

void *read_files(void *thread_data) {
    thread_reader_data *data = (thread_reader_data *) thread_data;
    if (print_thread_activity) {
        printf("[READER-%d] started...\n", data->thread_number);
    }
    do {
        char filename[7] = "lab1_0\0";
        filename[5] = '0' + data->file_number;
        int file_desc = -1;
        while (file_desc == -1) {
            if (observe_block) {
                printf("[READER-%d] wait for mutex %d...\n", data->thread_number, data->file_number);
            }
            fwait(&data->futexes[data->file_number]);
            if (!infinity_loop) {
                fpost(&data->futexes[data->file_number]);
                return NULL;
            }
            if (observe_block) {
                printf("[READER-%d] captured mutex %d!\n", data->thread_number, data->file_number);
            }
            file_desc = open(filename, O_RDONLY, 00666);
            if (file_desc == -1) {
                fpost(&data->futexes[data->file_number]);
                if (observe_block) {
                    printf("[READER-%d] free mutex %d!\n", data->thread_number, data->file_number);
                }
                if (print_thread_activity) {
                    printf("[READER-%d] I/O error on open file %s.\n", data->thread_number, filename);
                }
            }
        }
        struct stat st;
        stat(filename, &st);
        int file_size = st.st_size;

        char *buffer = seq_read(file_desc, file_size);
        close(file_desc);
        fpost(&data->futexes[data->file_number]);
        if (observe_block) {
            printf("[READER-%d] free mutex %d!\n", data->thread_number, data->file_number);
        }
        int *int_buf = (int *) buffer;
        long sum = 0;
        for (int i = 0; i < file_size / sizeof(int); i++) {
            sum += int_buf[i];
        }

        if (print_sum) {
            printf("[READER-%d] file %s sum is %ld.\n", data->thread_number, filename, sum);
        }

        free(buffer);
    } while (infinity_loop);
    return NULL;
}

void seq_write(void *ptr, int size, int n, int fd, const char* filepath, int file_offset) {
    struct stat fstat;
    stat(filepath, &fstat);
    int blksize = (int) fstat.st_blksize;
    int align = blksize-1;
    int bytes = size * n;
    // impossible to use G from the task because O_DIRECT flag requires aligned both the memory address and your buffer to the filesystem's block size
    int blocks = bytes / blksize;

    char *buff = (char *) malloc((int)blksize+align);
    char *wbuff = (char *)(((uintptr_t)buff+align)&~((uintptr_t)align));

    for (int i = 0; i < blocks; ++i) {
        char* buf_ptr = (char *)ptr + blksize*i;
        // copy from memory to write buffer
		memcpy(wbuff, buf_ptr, blksize);
        if (pwrite(fd, wbuff, blksize, blksize*i + file_offset) < 0) {
            free(buff);
            printf("write error occurred\n");
            return;
        }
    }
    free(buff);
}

void *write_to_files(void *thread_data) {
    thread_writer_data *data = (thread_writer_data *) thread_data;
    int *write_pointer = data->start;
    if (print_thread_activity) {
        printf("[WRITER] started...\n");
    }
    do {
        for (int i = 0; i < data->files; i++) {
            char filename[7] = "lab1_0\0";
            filename[5] = '0' + i;
            if (observe_block) {
                printf("[WRITER] waiting for mutex %d...\n", i);
            }
            fwait(&data->futexes[i]);
            if (observe_block) {
                printf("[WRITER] captured mutex %d\n", i);
            }
            // NOCACHE file write
            int current_file = open(filename, O_WRONLY | O_CREAT | __O_DIRECT, 00666);
            if (current_file == - 1) {
                printf("error on open file for write\n");
                fpost(&data->futexes[i]);
                return NULL;
            }
            int ints_to_file = data->ints_per_file;
            int is_done = 0;
            if (print_thread_activity) {
                printf("[WRITER] write started\n");
            }
            int file_offset = 0;
            while (!is_done) {
                if (ints_to_file + write_pointer < data->end) {
                    seq_write(write_pointer, sizeof(int), ints_to_file, current_file, filename, file_offset);
                    write_pointer += ints_to_file;
                    is_done = 1;
                } else {
                    int available = data->end - write_pointer;
                    seq_write(write_pointer, sizeof(int), available, current_file, filename, file_offset);
                    write_pointer = data->start;
                    ints_to_file -= available;
                    file_offset += available * 4;
                }
            }
            close(current_file);
            if (print_thread_activity) {
                printf("[WRITER] write finished\n");
            }
            fpost(&data->futexes[i]);
            if (observe_block) {
                printf("[WRITER] free mutex %d\n", i);
            }
        }
    } while (infinity_loop);
    return NULL;
}

int main() {
    const char *devurandom_filename = "/dev/urandom\0";
    FILE *devurandom_file = fopen(devurandom_filename, "r");

    printf("До аллокации (продолжить - [ENTER])");
    getchar();
    int *memory_region = malloc(A * megabyte_size);
    int *thread_data_start = memory_region;

    // generator threads start

    pthread_t *generator_threads = (pthread_t *) malloc(D * sizeof(pthread_t));
    thread_generator_data *generator_data = (thread_generator_data *) malloc(D * sizeof(thread_generator_data));
    printf("После аллокации (продолжить - [ENTER])");
    getchar();

    int ints = A * 1024 * 256;
    int ints_per_thread = ints / D;
    for (int i = 0; i < D; ++i) {
        generator_data[i].thread_number = i;
        generator_data[i].ints_per_thread = ints_per_thread;
        generator_data[i].start = thread_data_start;
        generator_data[i].file = devurandom_file;
        thread_data_start += ints_per_thread;
    }
    generator_data[D - 1].ints_per_thread += ints % D;

    // generator threads end

    // writer thread start

    int files = A / E;
    if (A % E != 0) {
        files++;
    }

    int *futexes = malloc(sizeof(int) * files);
    for (int i = 0; i < files; ++i) {
        futexes[i] = 1;
    }

    thread_writer_data *writer_data = (thread_writer_data *) malloc(sizeof(thread_writer_data));
    pthread_t *thread_writer = (pthread_t *) malloc(sizeof(pthread_t));
    writer_data->ints_per_file = E * 1024 * 256;
    writer_data->files = files;
    writer_data->start = memory_region;
    writer_data->end = memory_region + ints;
    writer_data->futexes = futexes;

    // writer thread end

    // reader threads start

    pthread_t *reader_threads = (pthread_t *) malloc(I * sizeof(pthread_t));
    thread_reader_data *reader_data = (thread_reader_data *) malloc(I * sizeof(thread_reader_data));
    int file_number = 0;
    for (int i = 0; i < I; ++i) {
        if (file_number >= files) {
            file_number = 0;
        }
        reader_data[i].thread_number = i;
        reader_data[i].file_number = file_number;
        reader_data[i].futexes = futexes;
        file_number++;
    }

    // reader threads end

    for (int i = 0; i < D; ++i) {
        pthread_create(&(generator_threads[i]), NULL, fill_with_random, &generator_data[i]);
    }
    pthread_create(thread_writer, NULL, write_to_files, writer_data);
    for (int i = 0; i < I; ++i) {
        pthread_create(&(reader_threads[i]), NULL, read_files, &reader_data[i]);
    }

    printf("Для останвки беск. цикла нажмите [ENTER]\n");
    getchar();
    infinity_loop = 0;

    for (int i = 0; i < I; i++) {
        pthread_join(reader_threads[i], NULL);
    }
    pthread_join(*thread_writer, NULL);
    for (int i = 0; i < D; i++) {
        pthread_join(generator_threads[i], NULL);
    }

    free(futexes);

    fclose(devurandom_file);

    free(generator_threads);
    free(generator_data);
    free(thread_writer);
    free(writer_data);
    free(reader_threads);
    free(reader_data);
    free(memory_region);
    printf("После деаллокации");
    getchar();
    return 0;
}
