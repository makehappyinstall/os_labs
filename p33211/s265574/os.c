/*
 * Разработать программу на языке С, которая осуществляет следующие действия
 *
 * 1. Создает область памяти размером 133 мегабайт, начинающихся с адреса 0x095FA0F5
 * (если возможно) при помощи malloc, заполненную случайными числами /dev/urandom в
 * 62 потоков. Используя системные средства мониторинга определите адрес начала в
 * адресном пространстве процесса и характеристики выделенных участков памяти.
 *
 * Замеры виртуальной/физической памяти необходимо снять:
 *
 * - До аллокации
 * - После аллокации
 * - После заполнения участка данными
 * - После деаллокации
 *
 * 2. Записывает область памяти в файлы одинакового размера 160 мегабайт с использованием
 * блочного обращения к диску. Размер блока ввода-вывода 62 байт. Преподаватель выдает
 * в качестве задания последовательность записи/чтения блоков - случайный.
 * Генерацию данных и запись осуществлять в бесконечном цикле.
 *
 * 3. В отдельных 42 потоках осуществлять чтение данных из файлов и подсчитывать
 * агрегированные характеристики данных - минимальное значение.
 *
 * Чтение и запись данных в/из файла должна быть защищена примитивами синхронизации flock.
 * 
 * 4. По заданию преподавателя изменить приоритеты потоков и описать изменения в
 * характеристиках программы.
 * 
 * 5. Измерить значения затраченного процессорного времени на выполнение программы и на
 * операции ввода-вывода используя системные утилиты.
 *
 * 6. Отследить трассу системных вызовов.
 * 
 * 7. Используя stap построить графики системных характеристик.
 */

#define _GNU_SOURCE

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/file.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <sys/types.h>

#include <pthread.h>

#define MEMORY_ADDRESS 0x095FA0F5
#define MEMORY_SIZE (133 * 1024 * 1024) /* 133 MB */
#define WRITE_THREADS_COUNT 62
#define FILE_SIZE (160 * 1024 * 1024) /* 160 MB */
#define FILE_COUNT (MEMORY_SIZE / FILE_SIZE + 1)
#define IO_BLOCK_SIZE 62 /* 62 B */
#define READ_THREADS_COUNT 42

#define FILE_OPEN_FLAGS O_RDWR | O_CREAT
#define FILE_OPEN_MODE S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH
#define FILE_TEMPLATE "/tmp/os-rand-file-%lu"

static volatile int thread_initialized;
static volatile int thread_terminate = 0;

static pthread_t write_threads[WRITE_THREADS_COUNT];
static pthread_t read_threads[READ_THREADS_COUNT];

struct write_thread_args {
    char *start;
    size_t size;
};

struct read_thread_args {
    size_t num;
    off_t offset;
    size_t size;
};

static void fd_setup( int * tmp_fd ) {
    for (size_t i = 0; i < FILE_COUNT; i++) {     
        char filename[22];
        snprintf(filename, 22, FILE_TEMPLATE, i);

        /* create a sparse tmp file */
        tmp_fd[i] = open(filename, FILE_OPEN_FLAGS, FILE_OPEN_MODE);
        ftruncate(tmp_fd[i], FILE_SIZE);
    }
}

static void fd_destroy( int * tmp_fd ) {
    for (size_t i = 0; i < FILE_COUNT; i++) {
        close(tmp_fd[i]);
    }
}

static void do_write_block(size_t count, char *start, size_t size) {
    int tmp_fd[FILE_COUNT] = {0};
    fd_setup(tmp_fd);

    size_t lock_count = 0;
    thread_initialized = 1;
    while (!thread_terminate) {
        /* write random bytes into designated memory space */
        getrandom(start, size, 0);
        
        /* acquire a lock on a random file */
        size_t num = rand() % FILE_COUNT;
        flock(tmp_fd[num], LOCK_EX);

        /* if thread has to be terminated, unlock file and do not perform any actions */
        if (thread_terminate) {
            flock(tmp_fd[num], LOCK_UN);
            break;
        }

        lock_count++;
        fprintf(stderr, "[write %3lu] ACQUIRE (for the %lu time)\n", count, lock_count);

        /* write bytes from memory space to random offsets in the file */
        for (size_t i = 0; i < size; i += IO_BLOCK_SIZE) {
            off_t offset = rand() % (FILE_SIZE - IO_BLOCK_SIZE);
            lseek(tmp_fd[num], offset, SEEK_SET);
            write(tmp_fd[num], start + i, IO_BLOCK_SIZE);
        }

        /* release lock */
        fprintf(stderr, "[write %3lu] RELEASE\n", count);
        flock(tmp_fd[num], LOCK_UN);
    }
    fd_destroy(tmp_fd);
    fprintf(stderr, "[write %3lu] Thread terminated\n", count);
}

static void *write_block(void *args) {
    static size_t count = 0;

    struct write_thread_args *v_args = args;
    do_write_block(++count, v_args->start, v_args->size);

    return NULL;
}

static void write_threads_init(char *p) {
    /* give each thread a dedicated memory space */
    size_t size = MEMORY_SIZE / WRITE_THREADS_COUNT;

    for (size_t i = 0; i < WRITE_THREADS_COUNT; i++) {
        thread_initialized = 0;

        struct write_thread_args args = {p + i * size, size};
        pthread_create(&write_threads[i], NULL, write_block, &args);
        while (!thread_initialized);
        fprintf(stderr, "%3lu. Created write thread for block at %ld\n", (i + 1), (long) (p + i * size));
    }
}

static void do_read_block(size_t count, size_t num, off_t offset, size_t size) {
    int tmp_fd[FILE_COUNT] = {0};
    fd_setup(tmp_fd);

    size_t lock_count = 0;
    thread_initialized = 1;
    while (!thread_terminate) {
        /* exclusive file lock */
        flock(tmp_fd[num], LOCK_EX);

        /* if thread has to be terminated, unlock file and do not perform any actions */
        if (thread_terminate) {
            flock(tmp_fd[num], LOCK_UN);
            break;
        }

        lock_count++;
        fprintf(stderr, "[read %4lu] ACQUIRE (for the %lu time)\n", count, lock_count);

        /* read a block at the offset */
        unsigned int block[size / 4];
        lseek(tmp_fd[num], offset, SEEK_SET);
        read(tmp_fd[num], block, size);

        unsigned int min = UINT_MAX;
        for (size_t i = 0; i < size / 4; i++) {
            if (min > block[i]) min = block[i];
        }
        

        /* release lock */
        fprintf(stderr, "[read %4lu] RELEASE, aggregated result: %u\n", count, min);
        flock(tmp_fd[num], LOCK_UN);

        /* read threads are too lightweight, they are eating cpu time. Let them rest for at least 1 us */
        usleep(1);
    }
    fd_destroy(tmp_fd);
    fprintf(stderr, "[read %4lu] Thread terminated\n", count);
}

static void *read_block(void *args) {
    static size_t count = 0;

    struct read_thread_args *v_args = args;        
    do_read_block(++count, v_args->num, v_args->offset, v_args->size);

    return NULL;
}

static void read_threads_init(void) {
    size_t threads_per_file = READ_THREADS_COUNT / FILE_COUNT;
    size_t size = FILE_SIZE / threads_per_file;

    for (size_t i = 0; i < FILE_COUNT; i++) {
        for (size_t j = 0; j < threads_per_file; j++) {
            thread_initialized = 0;
            struct read_thread_args args = {i, (j * size), size};

            pthread_create(&read_threads[i], NULL, read_block, &args);
            while (!thread_initialized);
            fprintf(stderr, "%3lu. Created read thread, offset %ld, size %lu\n", (i * threads_per_file + j + 1), (j * size), size);
        }
    }
}

static void close_threads(void) {
    thread_terminate = 1;
    for (size_t i = 0; i < WRITE_THREADS_COUNT; i++) pthread_join(write_threads[i], NULL);
    for (size_t i = 0; i < READ_THREADS_COUNT; i++) pthread_join(read_threads[i], NULL);
    puts("All threads are terminated");
}

int main( void ) {
    srand((unsigned) time(NULL));

    while (getchar() != '\n'); /* Before allocation */
    char *p = malloc(MEMORY_SIZE);
    puts("Memory allocated");
    while (getchar() != '\n'); /* After allocation */

    write_threads_init(p);
    read_threads_init();

    while (getchar() != '\n'); /* After memory filling (need to wait a little) */
    close_threads();

    munmap(p, MEMORY_SIZE);
    while (getchar() != '\n'); /* After deallocating */
    return 0;
}
