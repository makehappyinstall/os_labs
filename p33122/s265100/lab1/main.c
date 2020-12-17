#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <values.h>
#include "CpuTime.h"

int size = 112 * 1024 * 1024;
int num_of_threads_random = 32;
int size_of_file = 29 * 1024 * 1024;
int size_of_block = 142;
int num_of_threads_agregate = 30;
int part = 0;
char *array;
int num_of_files;
int fd[];
int min_value = MAXINT;
char c;
double io_time = 0, io_time_start = 0, io_time_end = 0, time_start = 0, time_end = 0;

typedef struct args_for_random_tag {
    int id;
} args_for_random;

typedef struct args_for_agr_tag {
    int id;
    int min;
} args_for_agr;

void *fill_array(void *arg) {
    args_for_random *args = (args_for_random *) arg;
    int one_thread_should_write = size / num_of_threads_random;
    int start = args->id * one_thread_should_write;
    int randomData = open("/dev/urandom", O_RDONLY);

    if (randomData < 0) {
        printf("Could not access urandom\n");
        exit(1);
    } else {
        ssize_t result;
        if (part != num_of_threads_random)
            result = read(randomData, &array[start], one_thread_should_write);
        else
            result = read(randomData, &array[start], size - (one_thread_should_write * (num_of_threads_random - 1)));
        if (result < 0) {
            printf("Could not read from urandom\n");
        }
    }
    close(randomData);
    return 0;
}

void *agr_array(void *arg) {
    args_for_agr *args = (args_for_agr *) arg;
    int min = MAXINT;
    int temp = size_of_file;
    for (int i = 0; i < num_of_files; i++) {
        if (i == num_of_files - 1) {
            int file_act_size = size - size_of_file * (num_of_files - 1);
            size_of_file = file_act_size;
        }
        int data_size;
        if (args->id != num_of_threads_agregate - 1) {
            data_size = size_of_file / num_of_threads_agregate;
        } else {
            data_size = size_of_file - ((num_of_threads_agregate - 1) * (size_of_file / num_of_threads_agregate));
        }
        io_time_start = getCPUTime();
        char buf[data_size];
        ssize_t bytes_read = pread(fd[i], buf, data_size, (size_of_file / num_of_threads_agregate) * args->id);
        io_time_end = getCPUTime();
        io_time += io_time_end - io_time_start;
        for (int j = 0; j < sizeof(buf); j++) {
            if (buf[j] < min) {
                min = buf[j];
            }
        }
        if (args->id == num_of_threads_agregate - 1) {
            flock(fd[i], LOCK_UN);
            close(fd[i]);
        }
    }
    size_of_file = temp;
    args->min = min;
    return 0;
}

int main() {
//    while (1) {
    time_start = getCPUTime();
    printf("Press any button to start\n");
    while ((c = getchar()) != '\n' && c != EOF);
    printf("After entering a character the program allocates memory\n");
    getchar();
    array = (char *) malloc(size);

    while ((c = getchar()) != '\n' && c != EOF);
    printf("After entering a character the program will write data to memory\n");
    getchar();
    pthread_t threads[num_of_threads_random];
    args_for_random args_read[num_of_threads_random];
    for (int i = 0; i < num_of_threads_random; i++) {
        args_read[i].id = i;
        pthread_create(&threads[i], NULL, fill_array, (void *) &args_read[i]);
    }

    for (int i = 0; i < num_of_threads_random; i++)
        pthread_join(threads[i], NULL);

    while ((c = getchar()) != '\n' && c != EOF);
    printf("After entering the character the program will write data to a file\n");
    getchar();

    if (size % size_of_file == 0) {
        num_of_files = size / size_of_file;
    } else {
        num_of_files = size / size_of_file + 1;
    }
    printf("Number of files: %d\n", num_of_files);

    size_t wrote = 0;
    size_t sum = 0;
    int j = 0;
    io_time_start = getCPUTime();
    for (int i = 1; i < num_of_files + 1; i++) {
        char filename[sizeof "file1"];
        sprintf(filename, "file%d", i);
        int file = open(filename, O_RDWR | O_CREAT, 0666);

        flock(file, LOCK_EX);

        while ((wrote < size_of_file) && (sum < size)) {
            size_t bytes_wrote;

            if (wrote + size_of_block > size_of_file)
                bytes_wrote = write(file, &array[j * size_of_block], size_of_file - wrote);
            else if (sum + size_of_block > size)
                bytes_wrote = write(file, &array[j * size_of_block], size - sum);
            else
                bytes_wrote = write(file, &array[j * size_of_block], size_of_block);

            if (bytes_wrote >= 0) {
                wrote += bytes_wrote;
                sum += bytes_wrote;
            } else {
                printf("Something went wrong during writing");
            }
            j++;
        }

        flock(file, LOCK_UN);
        wrote = 0;
        close(file);
    }
    io_time_end = getCPUTime();
    io_time += io_time_end - io_time_start;
    free(array);

    pthread_t threads_agr[num_of_threads_agregate];
    args_for_agr args_agr[num_of_threads_agregate];
    for (int i = 1; i < num_of_files + 1; i++) {
        char filename[sizeof "file1"];
        sprintf(filename, "file%d", i);
        int file = open(filename, O_RDWR);
        flock(file, LOCK_SH);
        fd[i - 1] = file;
    }

    for (int i = 0; i < num_of_threads_agregate; i++) {
        args_agr[i].id = i;
        pthread_create(&threads_agr[i], NULL, agr_array, (void *) &args_agr[i]);
    }

    for (int i = 0; i < num_of_threads_agregate; i++)
        pthread_join(threads_agr[i], NULL);

    for (int i = 0; i < num_of_threads_agregate; i++) {
        if (args_agr[i].min < min_value) {
            min_value = args_agr[i].min;
        }
    }
    time_end = getCPUTime();
    printf("Затраченное время на выполнение программы: %lf\n", (time_end - time_start));
    printf("Затраченное время на операции ввода-вывода: %lf\n", io_time);
//    }
    return 0;
}
