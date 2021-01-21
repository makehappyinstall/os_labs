#include <stdio.h>
#include <sys/mman.h>
#include <zconf.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>

#define A 68
#define B 0xDD95BF70
#define C mmap
#define D 120
#define E 51
#define F block
#define G 96
#define H random
#define I 88
#define J max
#define K sem

typedef struct {
    unsigned char rand_fd;
    int start_ptr;
    int end_ptr;
    unsigned char *mem_ptr;
} mem_write_meta;

typedef struct {
    size_t f_amount;
    size_t f_chunk_size;
    size_t f_chunk_tail;
    unsigned char *mem_ptr;
    int *f_ds;
} file_write_meta;

typedef struct {
    int f_d;
    int index;
} file_read_meta;

pthread_t t_file_write_meta;

sem_t semaphore;
unsigned char *mem_ptr;
int bytes;
int rand_fd;

int r_index;
char r_data[10000];

int f_amount;
int *f_ds;

void pause_() {
    printf("Press enter to continue...\n");
    fflush(stdout);
    int c;
    while ((c = getchar()) != 0x0a);
    if (c != EOF) {
        return;
    }
    if (feof(stdin)) {
        exit(0);
    }
}

void mem_preallocate() {
    bytes = A * 1024 * 1024;
    rand_fd = open("/dev/urandom", O_RDONLY);
    mem_ptr = mmap(
            (void *) B,
            bytes,
            PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_ANONYMOUS,
            -1,
            0);

    if (mem_ptr == MAP_FAILED) {
        perror("Can't map a file\n");
        exit(-1);
    }
}

unsigned char r_read(int r_data_fd) {
    if (r_index < 10000) {
        r_index++;
        return r_data[r_index];
    } else {
        int result = read(r_data_fd, &r_data, sizeof(r_data));
        if (result == -1) {
            perror("Can't read int from /dev/urandom\n");
            return 0;
        }
        r_index = 0;
        return r_data[r_index];
    }
}

void *t_mem_write(void *t_mw_meta) {
    mem_write_meta *mw_meta = (mem_write_meta *) t_mw_meta;
    unsigned int bytes_done = 0;
    for (int i = mw_meta->start_ptr; i < mw_meta->end_ptr; i++) {
        unsigned char r = r_read(mw_meta->rand_fd);
        mw_meta->mem_ptr[i] = r;
        bytes_done++;
    }
    printf("Thread-#%d finished writing %uB\n", (int) pthread_self(), bytes_done);
    free(mw_meta);
    return NULL;
}

void mem_write() {
    const int chunk = bytes / D / sizeof(*mem_ptr);
    const int tail = bytes % D / sizeof(*mem_ptr);

    printf("Chunk size: %dB\n", chunk);
    printf("Chunk tail size: %dB\n", tail);

    pthread_t pthread_pool[D];
    mem_write_meta *mw_meta;
    int thread_counter = 0;
    for (int i = 0; i < D;) {
        mw_meta = malloc(sizeof(*mw_meta));
        mw_meta->mem_ptr = mem_ptr;
        mw_meta->rand_fd = rand_fd;
        mw_meta->start_ptr = i * chunk;
        mw_meta->end_ptr = ++i * chunk;
        if (pthread_create(&pthread_pool[thread_counter], 0, t_mem_write, (void *) mw_meta)) {
            free(mw_meta);
            printf("Can't create Thread\n");
        }
        thread_counter++;
    }

    if (tail != 0) {
        mw_meta = malloc(sizeof(*mw_meta));
        mw_meta->mem_ptr = mem_ptr;
        mw_meta->rand_fd = rand_fd;
        mw_meta->start_ptr = D * chunk;
        mw_meta->end_ptr = D * chunk + tail;
        t_mem_write((void *) mw_meta);
    }
    for (int i = 0; i < D; i++) {
        pthread_join(pthread_pool[i], NULL);
    }

}


void t_single_file_write(const unsigned char *ptr, int f_d, size_t n, size_t byte_c) {
    unsigned char block[G];
    int iterator = 0;
    int total_w = 0;
    size_t complete_block;
    size_t i;
    for (i = n * E; i < n * E + byte_c; i++) {
        block[iterator] = ptr[i];
        iterator++;
        if (iterator >= G) {
            iterator = 0;
            complete_block = write(f_d, &block, G);
            if (complete_block == -1) {
                perror("Can't write to destination\n");
                return;
            }
            total_w += complete_block;
        }
    }

    if (iterator > 0) {
        complete_block = write(f_d, &block, G - iterator);
        if (complete_block == -1) {
            perror("Can't write to destination\n");
            return;
        }
        total_w += complete_block;
    }
    printf("[%zu] total written: %d B\n", n, total_w);

}

void *t_file_write(void *t_fw_meta) {
    file_write_meta *fw_meta = (file_write_meta *) t_fw_meta;
    printf("File chunk size: %zu\n", fw_meta->f_chunk_size);
    while (1) {
        sem_wait(&semaphore);
        printf("[Locked] Writing to files\n");

        for (int i = 0; i < fw_meta->f_amount; i++) {
            if (fw_meta->f_chunk_tail != 0 && i == fw_meta->f_amount - 1) {
                t_single_file_write(
                        fw_meta->mem_ptr,
                        fw_meta->f_ds[i],
                        i,
                        fw_meta->f_chunk_tail);
            } else {
                t_single_file_write(
                        fw_meta->mem_ptr,
                        fw_meta->f_ds[i],
                        i,
                        fw_meta->f_chunk_size);
            }
        }
        sem_post(&semaphore);
        printf("[Unlocked] Writing is done\n\n");
    }
}

void file_write() {
    f_amount = A / E + 1;
    f_ds = malloc(sizeof(int) * f_amount);

    srand(time(NULL));
    char filename[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    for (unsigned int j = 0; j < 9; ++j) {
        filename[j] = random() % ('z' - 'A') + 'A';
    }
    char f_name[16];
    for (int i = 0; i < f_amount; i++) {
        sprintf(f_name, "%s_%d.bin", filename, i);
        f_ds[i] = open(f_name, O_RDWR | O_CREAT, (mode_t) 0600);
        if (f_ds[i] == -1) {
            perror("Can't open file\n");
            exit(-1);
        }
    }

    file_write_meta *fw_meta = malloc(sizeof(file_write_meta));

    const int tail = bytes % f_amount;
    const int chunk = bytes / f_amount;

    fw_meta->f_amount = f_amount;
    fw_meta->mem_ptr = mem_ptr;
    fw_meta->f_chunk_size = chunk;
    fw_meta->f_chunk_tail = tail;
    fw_meta->f_ds = f_ds;

    if (pthread_create(&t_file_write_meta, NULL, t_file_write, (void *) fw_meta)) {
        free(fw_meta);
        perror("Can't create Thread\n");
    }
}


unsigned char MAX_AGGREGATE_VALUE;

void *t_file_read(void *t_fr_meta) {
    file_read_meta *fr_meta = (file_read_meta *) t_fr_meta;
    unsigned char IO_BLOCK[G];
    int read_b;

    sem_wait(&semaphore);
    printf("[Locked-%d] Reading has been started\n", fr_meta->index);

    if (lseek(fr_meta->f_d, 0, SEEK_SET) == -1) {
        return NULL;
    }

    while (1) {
        read_b = read(fr_meta->f_d, &IO_BLOCK, G);
        if (read_b == -1) {
            perror("Can't read. CAN'T SEE >_<\n");
            break;
        }
        if (read_b < G) {
            if (read_b == 0) {
                break;
            }
        } else {
            for (int i = 0; i < G; i++) {
                if (IO_BLOCK[i] > MAX_AGGREGATE_VALUE) {
                    MAX_AGGREGATE_VALUE = IO_BLOCK[i];
                }
            }
        }

    }
    sem_post(&semaphore);
    printf("[Unlocked-%d] Reading is done.\n\n", fr_meta->index);

    return NULL;
}

void aggregate() {
    pthread_t read_pthread_pool[I];
    int index = 0;
    for (int i = 0; i < I; i++) {
        file_read_meta *fr_meta = malloc(sizeof(file_read_meta));
        if (index >= f_amount)
            index = 0;
        fr_meta->f_d = f_ds[index];
        fr_meta->index = i;
        index++;
        if (pthread_create(&read_pthread_pool[i], NULL, t_file_read, (void *) fr_meta)) {
            free(fr_meta);
            perror("Can't create Thread\n");
        }
    }

    for (int i = 0; i < I; i++) {
        pthread_join(read_pthread_pool[i], NULL);
    }

    printf("MAX VALUE IS: %d\n", MAX_AGGREGATE_VALUE);
}

void exit_() {
    pthread_cancel(t_file_write_meta);
    for (int i = 0; i < f_amount; i++) {
        close(f_ds[i]);
    }
    if (close(rand_fd))
        perror("Error closing file\n");

    if (munmap(mem_ptr, bytes) == -1) {
        perror("Error during unmapping\n");
        close(rand_fd);
        exit(-1);
    }
    printf("Memory at [%p] got successfully unmapped\n", (void *) B);


}

int main() {
    sem_init(&semaphore, 0, 1);

    mem_preallocate();
    printf("Memory successfully allocated at [%p]\n", (void *) B);
    pause_();

    mem_write();
    printf("Memory at [%p] got successfully fulfilled with %d MiB in %d Threads\n", (void *) B, A, D);
    pause_();

    printf("Writing started\n");
    file_write();
    pause_();

    aggregate();
    printf("Work is done. Unmapping memory at [%p]\n", (void *) B);
    exit_();
    return 0;
}
