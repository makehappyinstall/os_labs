
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <pthread.h>

#include "thread_pool.h"

// A=355;B=0x15ADFE03;C=mmap;D=58;E=116;F=block;G=26;H=random;I=73;J=sum;K=sem
#define A 355
#define B 0x15ADFE03
#define C mmap
#define D 58
#define E 116
#define F block
#define G 26
#define H random
#define I 73
#define J sum
#define K sem

#define Q(x) #x
#define QUOTE(x) Q(x)

typedef struct {
    unsigned char * start_address;
    size_t length;
    FILE * urandom;
} fill_thread_data_t;

typedef struct {
    const char * filename;
    sem_t * file_semaphores;
} read_thread_data_t;

typedef struct {
    char filename[256];
    long result;
    tpool_t * tm;
    sem_t * sem;
} read_work_data_t;

typedef struct {
    char * buf;
    size_t length;
    size_t offset;
    long result;
    tpool_t * tm;
} sum_work_data_t;

void pause() {
    printf("Press enter to continue...");
    fflush(stdout);

    int c;
    while ((c = getchar()) != 0x0a);
    if (c != EOF) {
        return;
    }

    if (ferror(stdin)) {
        perror("ferror");
        exit(-5);
    }

    if (feof(stdin)) {
        exit(0);
    }
}

void fill(const fill_thread_data_t data) {
    for (size_t i = 0; i < data.length; ) {
        i += fread(data.start_address + i, 1, data.length - i, data.urandom);

        if (ferror(data.urandom)) {
            perror("ferror");
            exit(-3);
        }

        if (feof(data.urandom)) {
            fprintf(stderr, "feof\n");
            exit(-3);
        }
    }

    printf(".");
    fflush(stdout);
}

void * fill_thread(const fill_thread_data_t * data) {
    fill(*data);
    return NULL;
}

void gen_and_write(const char * filename, sem_t file_semaphores[]) {
    pause();

    printf("Allocating %dMiB of memory at the address %p...\n", A, (void *) B);
    unsigned char * memory = (unsigned char *) C((void *) B, A * 1024 * 1024, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (memory == (void *) -1) {
        switch (errno) {
            case EACCES:
                fprintf(stderr, "EACCES\n");
                break;

            case EAGAIN:
                fprintf(stderr, "EAGAIN\n");
                break;

            case EBADF:
                fprintf(stderr, "EBADF\n");
                break;

            case EINVAL:
                fprintf(stderr, "EINVAL\n");
                break;

            case ENFILE:
                fprintf(stderr, "ENFILE\n");
                break;

            case ENODEV:
                fprintf(stderr, "ENODEV\n");
                break;

            case ENOMEM:
                fprintf(stderr, "ENOMEM\n");
                break;

            case EOVERFLOW:
                fprintf(stderr, "EOVERFLOW\n");
                break;
        }

        exit(-1);
    }
    pause();

    printf("Filling allocated at %p memory from /dev/urandom in %d fill_threads...\n", memory, D);
    FILE * urandom = fopen("/dev/urandom", "rb");

    if (urandom == NULL) {
        fprintf(stderr, "Cannot open urandom\n");
        exit(-6);
    }

    pthread_t fill_threads[D - 1];
    fill_thread_data_t fill_threads_data[D];
    unsigned char * next_address = memory;
    unsigned int part = A * 1024 * 1024 / D;
    for (unsigned int i = 0; i < D - 1; ++i) {
        fill_threads_data[i].start_address = next_address;
        fill_threads_data[i].length = part;
        fill_threads_data[i].urandom = urandom;

        int err = pthread_create(fill_threads + i, NULL, (void* (*)(void *)) fill_thread, (void *) (fill_threads_data + i));

        switch (err) {
            case EAGAIN:
                fprintf(stderr, "EAGAIN\n");
                exit(-2);

            case EINVAL:
                fprintf(stderr, "EINVAL\n");
                exit(-2);

            case EPERM:
                fprintf(stderr, "EPERM\n");
                exit(-2);
        }

        next_address += part;
    }

    fill_threads_data[D - 1].start_address = next_address;
    fill_threads_data[D - 1].length = memory + A * 1024 * 1024 - next_address;
    fill_threads_data[D - 1].urandom = urandom;
    fill(fill_threads_data[D - 1]);

    for (unsigned int i = 0; i < D - 1; ++i) {
        int err = pthread_join(fill_threads[i], NULL);

        switch (err) {
            case EDEADLK:
                fprintf(stderr, "EDEADLK\n");
                exit(-4);

            case EINVAL:
                fprintf(stderr, "EINVAL\n");
                exit(-4);

            case ESRCH:
                fprintf(stderr, "ESRCH\n");
                exit(-4);
        }
    }

    int urandom_err = fclose(urandom);
    switch (urandom_err) {
        case EBADF:
            fprintf(stderr, "EBADF\n");
            exit(-6);
    }

    printf("\n");
    pause();

    printf("Writing allocated memory into the files with name \"%s.n.bin\" %dMiB per file using "QUOTE(F)" disc accessing with block size %d bytes\n", filename, E, G);
    for (unsigned int i = 0, offset = 0; offset < A; ++i, offset += E) {
        char current_file[256];

        sem_wait(file_semaphores + i);

        snprintf(current_file, 256, "%s.%d.bin", filename, i);
        FILE * file = fopen(current_file, "wb");
        size_t file_size = A - offset;
        if (file_size > E) {
            file_size = E;
        }

        file_size *= 1024 * 1024;
        static unsigned char buf[E * 1024 * 1024];
        memset(buf, 0, file_size);

        for (unsigned int c = 0; c < file_size; ) {
            c += fwrite(buf + c, 1, file_size - c, file);
        }

        int seek_err = fseek(file, 0, SEEK_END);
        switch (seek_err) {
            case EBADF:
                fprintf(stderr, "EBADF\n");
                exit(-7);

            case EINVAL:
                fprintf(stderr, "EINVAL\n");
                exit(-7);
        }

        for (size_t blk_offset = 0; blk_offset < file_size; ) {
            size_t cnt = file_size - blk_offset;

            if (cnt > G) {
                cnt = G;
            }

            seek_err = fseek(file, -cnt, SEEK_CUR);
            switch (seek_err) {
                case EBADF:
                    fprintf(stderr, "EBADF\n");
                    exit(-7);

                case EINVAL:
                    fprintf(stderr, "EINVAL\n");
                    exit(-7);
            }

            for (size_t blk_written = 0; blk_written < cnt; ) {
                blk_written += fwrite(memory + offset * 1024 * 1024 + file_size - blk_offset - cnt + blk_written, 1, cnt - blk_written, file);
            }

            seek_err = fseek(file, -cnt, SEEK_CUR);
            switch (seek_err) {
                case EBADF:
                    fprintf(stderr, "EBADF\n");
                    exit(-7);

                case EINVAL:
                    fprintf(stderr, "EINVAL\n");
                    exit(-7);
            }

            blk_offset += cnt;
            printf("%s\t%lu bytes from %lu written\r", current_file, blk_offset, file_size);
        }
        printf("\n");

        if (ferror(stdin)) {
            perror("ferror");
            exit(-8);
        }

        int err = fclose(file);
        switch (err) {
            case EBADF:
                fprintf(stderr, "EBADF\n");
                exit(-9);
        }

        sem_post(file_semaphores + i);
    }

    printf("Unmap memory\n");
    munmap(memory, A * 1024 * 1024);
    pause();
}

void read_sum_work(void * work_data) {
    sum_work_data_t * data = work_data;

    for (size_t i = 0, offset = data->offset; i < data->length; ++i, ++offset) {
        data->result += data->buf[offset];
    }
}

void read_file_work(void * work_data) {
    read_work_data_t * data = work_data;

    char buf[2048];
    sum_work_data_t sum_data = { buf, 2048, 0, 0, data->tm };

    sem_wait(data->sem);

    FILE * file = fopen(data->filename, "rb");
    while (fread(buf, 1, 2048, file) > 0) {
        tpool_wait_work(tpool_add_work(data->tm, read_sum_work, &sum_data, true));
        data->result += sum_data.result;
    }

    fclose(file);
    sem_post(data->sem);
}

void * read_main_thread(void * thread_data) {
    read_thread_data_t * data = thread_data;
    tpool_t * tm = tpool_create(I - 1);

    while (1) {
        const unsigned int filenames_count = A / E + (A % E > 0 ? 1 : 0);

        read_work_data_t work_datas[filenames_count];
        for (unsigned int i = 0; i < filenames_count; ++i) {
            snprintf(work_datas[i].filename, 256, "%s.%d.bin", data->filename, i);
            work_datas[i].result = 0;
            work_datas[i].tm = tm;
            work_datas[i].sem = data->file_semaphores + i;

            tpool_add_work(tm, read_file_work, work_datas + i, false);
        }

        tpool_wait(tm);
        int result = 0;
        for (unsigned int i = 0; i < filenames_count; ++i) {
            result += work_datas[i].result;
        }

        printf("Sum: %d\n", result);
    }
}

int main() {
    srand(time(NULL));
    char filename[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    for (unsigned int i = 0; i < 9; ++i) {
        filename[i] = random() % ('z' - 'A') + 'A';
    }

    printf("Filename is \"%s\"\n", filename);


    unsigned int file_semaphores_count = A / E + (A % E > 0 ? 1 : 0);
    sem_t file_semaphores[file_semaphores_count];

    for (unsigned int i = 0; i < file_semaphores_count; ++i) {
        sem_init(file_semaphores + i, 0, 1);
    }

    gen_and_write(filename, file_semaphores);

    pthread_t read_thread;
    read_thread_data_t read_thread_data = { filename, file_semaphores };
    pthread_create(&read_thread, NULL, read_main_thread, &read_thread_data);

    while (1) {
        gen_and_write(filename, file_semaphores);
    }

    return 0;
}
