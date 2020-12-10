#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdnoreturn.h>
#include <errno.h>

// For O_DIRECT
#define __USE_GNU 1

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
#include <semaphore.h>
#include <getopt.h>


int A = 359; // размер (мегабайт)
size_t B = 0x99B3317C; // адрес начала
// C - malloc
int D = 73; // кол-во потоков для заполнения
int E = 43; // размер файлов одинакового размера (мегабайт)
// F - nocache
int G = 107; // размер блока ввода-вывода (байт)
// H - последовательный
int I = 50; // кол-во потоков, осуществляющих чтение файлов и подсчитывающих агрегированные характеристики данных
// J - среднее значение
// K - примитив синхронизации - sem

#define READ_BUF_INTS_NUMBER 250

int urandom_fd;
sem_t semaphore;

bool first_time = true;

size_t allocate_size;
size_t file_size;

// Options
bool run_once = false;
bool will_pause = false;
bool print_buffer = false;
char *file_path_format = "tmp/file%d";


void pause_until_stdin(const char *str) {
    if (first_time && will_pause) {
        printf("%s\n", str);
        getchar();
    }
}

struct fill_task {
    char *buf;
    size_t offset;
    size_t length;
};
struct count_task {
	size_t parts;
	size_t file_path_length;
};

void* fill_buffer(void *task_p) {
    struct fill_task *task = (struct fill_task *) task_p;
    size_t size = 1e3;

    size_t rest = task->length;
    while (rest > 0) {
        size_t bs = (size > rest ? rest : size);
        read(urandom_fd, task->buf + task->offset + task->length - rest, bs);
        rest -= bs;
    }
    return task_p;
}

noreturn void* count_avg(void *taskp) {
	struct count_task *task = (struct count_task *) taskp;
	char file_path[task->file_path_length];

    long double avg = 0;
    const size_t buf_size = READ_BUF_INTS_NUMBER * sizeof(int);
    int *buf = malloc(buf_size);

    while (true) {
        avg = 0.0;
        for (int i = 0; i < task->parts; i++) {
            sprintf(file_path, file_path_format, i);
            int file_fd = open(file_path, O_RDONLY);
            sem_wait(&semaphore);

            struct stat st;
            stat(file_path, &st);

            size_t numbers = st.st_size / buf_size;
            for (size_t j = 0; j < numbers; j++) {
                int ret = read(file_fd, buf, buf_size);
                if (ret < 1) {
                    printf("Failed to read file util the end :(");
                    break;
                }
                for (int k = 0; k < READ_BUF_INTS_NUMBER; k++) {
                    avg += (double)(*(buf + k) / (double)numbers);
                }
            }

            sem_post(&semaphore);
            close(file_fd);
        }
    }
    free(buf);
}



int main(int argc, char **argv) {
    char *buffer;
    bool counting = false;
    pthread_t generator_threads[D];
    pthread_t counter_threads[I];

    int c;
    while ( (c = getopt(argc, argv, "bof:ph")) != -1) {
        switch (c) {
            case 'b':
                print_buffer = true;
                printf("Will print buffer address\n");
                break;
            case 'o':
                run_once = true;
                printf("Running once\n");
                break;
            case 'f':
                {
                    unsigned long len = strlen(optarg);
                    file_path_format = malloc(sizeof(char) * (len + 8));
                    sprintf(file_path_format, "%s/file%%d", optarg);

                    printf("Writing to '%s'\n", optarg);
                }
                break;
            case 'p':
                will_pause = true;
                printf("Will pause on every step\n");
                break;
            case 'h':
            default:
                printf("Usage: %s\n\n"
                       "-f dir - directory to write files into\n"
                       "-b - print buffer location\n"
                       "-o - run once\n"
                       "-p - make pauses on first iteration\n"
                        , argv[0]);
                return c != 'h';
        }
    }

    printf("Started as pid: %d\n", getpid());

    pause_until_stdin("Pre-allocate");

    urandom_fd = open("/dev/urandom", O_RDONLY);
    sem_init(&semaphore, 0, 1);

    allocate_size = A * 1e6;
    file_size = E * 1e6;

    size_t file_path_length = strlen(file_path_format) + 1;
    size_t parts = allocate_size / file_size;
    while(parts > 0) {
        file_path_length++;
        parts /= 10;
    }
    parts = allocate_size / file_size;

    char file_path[file_path_length];
    while(true) {
        buffer = malloc(allocate_size);
        if (print_buffer) {
            printf("Buffer at: %p\n", buffer);
        }
        pause_until_stdin("Allocated");

        for (int i = 0; i < D; i++) {
            struct fill_task *task = malloc(sizeof(struct fill_task));
            task->buf = buffer;
            task->length = (allocate_size / D);
            task->offset = i * task->length;
            int ret = pthread_create(&generator_threads[i], NULL, fill_buffer, task);
            if (ret) {
                printf("Failed to create all generating writing threads\n");
                return 1;
            }
        }
        for (int i = 0; i < D; i++) {
            struct fill_task *task;
            if (pthread_join(generator_threads[i], (void **)&task)) {
                printf("Failed to join thread [%d]\n", i);
                return 2;
            }
            free(task);
        }
        pause_until_stdin("Filled");

        for (int i = 0; i < parts; i++) {
            sprintf(file_path, file_path_format, i);
            // O_DIRECT not working with ext4 and tmpfs, but works with ntfs-3g
            int file_fd = open(file_path, O_CREAT /*| O_DIRECT*/ | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWUSR);
            if (file_fd == -1) {
                printf("Failed to open: %s [%d]\n", file_path, errno);
                return 3;
            }
            sem_wait(&semaphore);

            for (int j = 0; j < file_size; j += G) {
                posix_fadvise(file_fd, 0, 0, POSIX_FADV_SEQUENTIAL);
                posix_fadvise(file_fd, 0, 0, POSIX_FADV_DONTNEED);
                int ret = write(file_fd, buffer + j, G);
                if (ret == -1) {
                    printf("Failed to write: %s (+%d) [%d]\n", file_path, j, errno);
                    return 4;
                }
            }
            sem_post(&semaphore);
            close(file_fd);
        }

        if (!counting) {
            for (int i = 0; i < I; i++) {
                struct count_task *task = malloc(sizeof(struct count_task));
                task->parts = parts;
                task->file_path_length = file_path_length;
                int ret = pthread_create(&counter_threads[i], NULL, count_avg, task);
                if (ret) {
                    printf("Failed to create all threads\n");
                    return 5;
                }
            }
            counting = true;
        }

        free(buffer);
        pause_until_stdin("After dealloc");

        first_time = false;
        if (run_once) {
            break;
        }
    }

    sem_destroy(&semaphore);
    close(urandom_fd);
    return 0;
}
