//A=223;B=0xDD6A8120;C=mmap;D=20;E=84;F=block;G=106;H=seq;I=88;J=sum;K=cv
// gcc -g main.c -o main -lpthread

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/random.h>
#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define ADDRESS 0xDD6A8120
#define SIZE 223 * 1024 * 1024
#define WRITE_THREADS_NUMBER 3
#define FILE_SIZE 84 * 1024 * 1024
#define NUMBER_OF_FILE (223 / 84 + 1)
#define BLOCK_SIZE 106
#define READ_THREADS_NUMBER 3
#define LOG_FILE "/mnt/c/Users/Admin/os-laba/log_memory.txt"
#define FILE_TEMPLATE "/mnt/c/Users/Admin/os-laba/os-%zu"

void create_open_files();

void close_files();

void *fill_segment_and_write_to_file(void *p);

void *calculate_sum_of_file();

void log_memory(char *);

const size_t size_for_one_thread = SIZE / WRITE_THREADS_NUMBER;
struct file_cond
{
    int fd;
    bool isFree;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
};
struct file_cond file_cond[NUMBER_OF_FILE];
static volatile bool terminate = false;

int main()
{
    remove(LOG_FILE);
    create_open_files();
    printf("Hi, PID: %d\nEnter anything to allocate memory\n", getpid());
    log_memory("Before allocation");
    getchar();

    char *p = (char *)mmap(
        (void *) ADDRESS,
        SIZE,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );
    puts("Memory successfully allocated");
    log_memory("After allocation");
    getchar();

    char *p_init = p;
    pthread_t write_threads[WRITE_THREADS_NUMBER];
    for (size_t i = 0; i < WRITE_THREADS_NUMBER; i++)
    {
        pthread_create(&write_threads[i], NULL, fill_segment_and_write_to_file, p);
        p += size_for_one_thread;
        
    }
    getchar();
    log_memory("After filling area");

    pthread_t read_threads[READ_THREADS_NUMBER];
    for (size_t i = 0; i < READ_THREADS_NUMBER; i++)
    {
        pthread_create(&read_threads[i], NULL, calculate_sum_of_file, NULL);
        printf("Created %zu read thread\n", i + 1);
        
    }
    getchar();

    terminate = true;
    for (size_t i = 0; i < WRITE_THREADS_NUMBER; i++)
    {
        pthread_join(write_threads[i], NULL);
    }
    for (size_t i = 0; i < READ_THREADS_NUMBER; i++)
    {
        pthread_join(read_threads[i], NULL);
    }
    munmap(p_init, SIZE);
    puts("Memory deallocated");
    log_memory("After deallocation");
    close_files();
    getchar();
    return 0;
}

void create_open_files()
{
    for (size_t i = 0; i < NUMBER_OF_FILE; i++)
    {
        char filename[32];
        sprintf(filename, FILE_TEMPLATE, i + 1);
        file_cond[i].fd = open(filename, O_RDWR | O_CREAT, S_IRWXU);
        file_cond[i].isFree = true;
        ftruncate(file_cond[i].fd, FILE_SIZE);
        printf("Create file " FILE_TEMPLATE "\n", i + 1);
    }
}

void close_files()
{
    for (size_t i = 0; i < NUMBER_OF_FILE; i++)
    {
        close(file_cond[i].fd);
    }
}

void *fill_segment_and_write_to_file(void *p)
{
    printf("Created write thread for %p\n", p);
    while (!terminate)
    {
        getrandom((char *)p, size_for_one_thread, 0);
        size_t nf = rand() % NUMBER_OF_FILE;
        pthread_mutex_lock(&file_cond[nf].mutex);
        while (!file_cond[nf].isFree)
        {
            pthread_cond_wait(&file_cond[nf].condition, &file_cond[nf].mutex);
        }
        if (terminate)
        {
            pthread_cond_signal(&file_cond[nf].condition);
            pthread_mutex_unlock(&file_cond[nf].mutex);
            return NULL;
        }
        file_cond[nf].isFree = false;
        printf("thread %p WRITE to file %zu\n", p, nf + 1);
        off_t offset = rand() % (FILE_SIZE - size_for_one_thread);
        lseek(file_cond[nf].fd, offset, SEEK_SET);
        for (size_t i = 0; i < size_for_one_thread; i += BLOCK_SIZE)
        {
            write(file_cond[nf].fd, p + i, BLOCK_SIZE);
        }
        file_cond[nf].isFree = true;
        printf("thread %p RELEASE file %zu\n", p, nf + 1);
        pthread_cond_signal(&file_cond[nf].condition);
        pthread_mutex_unlock(&file_cond[nf].mutex);
    }
    return NULL;
}

void *calculate_sum_of_file()
{
    while (!terminate)
    {
        size_t nf = rand() % NUMBER_OF_FILE;
        pthread_mutex_lock(&file_cond[nf].mutex);
        /*while (!file_cond[nf].isFree)
        {
            pthread_cond_wait(&file_cond[nf].condition, &file_cond[nf].mutex);
        }*/
        if (terminate)
        {
            pthread_cond_signal(&file_cond[nf].condition);
            pthread_mutex_unlock(&file_cond[nf].mutex);
            return NULL;
        }
        file_cond[nf].isFree = false;
        printf("READ for SUM file /os-%zu\n", nf + 1);
        uint64_t sum = 0;
        char buff[BLOCK_SIZE];
        const size_t number_of_blocks = FILE_SIZE / BLOCK_SIZE;
        lseek(file_cond[nf].fd, 0, SEEK_SET);
        for (size_t i = 0; i < number_of_blocks; i++)
        {
            read(file_cond[nf].fd, buff, BLOCK_SIZE);
            for (size_t j = 0; j < BLOCK_SIZE; j++)
            {
                sum += buff[j];
            }
        }
        file_cond[nf].isFree = true;
        printf("        RELEASE file tmp/os-%zu. SUM = %lu\n", nf + 1, sum);
        pthread_cond_signal(&file_cond[nf].condition);
        pthread_mutex_unlock(&file_cond[nf].mutex);
    }
    return NULL;
}

void write_status(char *str)
{
    FILE *f = fopen(LOG_FILE, "a");
    fputs(" ### ", f);
    fputs(str, f);
    fputs(" ###", f);
    fclose(f);
}

void write_to_log(char *str)
{
    FILE *f = fopen(LOG_FILE, "a");
    fputs(str, f);
    fclose(f);
}

void log_memory(char *status)
{
    write_status(status);
    write_to_log("\n\nfree -h\n");
    system("free -h >> " LOG_FILE);
    char cmd[61];
    sprintf(cmd, "cat /proc/%d/maps >>" LOG_FILE, getpid());
    write_to_log("\nmaps:\n");
    system(cmd);
    write_to_log("\n");
}
