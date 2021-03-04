#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <semaphore.h>

#define ALIGNMENT_BLOCK_SIZE 512

#define A (63 * 1048576)
void *B = (void *) 0x326B50C0;
#define E (77 * 1048576)
#define G 42
#define I 47
#define D 117
#define files_number I
#define true 1
#define false 0
char *ptr;
sem_t semaphore;

void *fill_area(void *data)
{
    int i = *(int *)data;

    int part_size = A / D;
    char *part_start = ptr + part_size * i;
    char *part_end = (i == D) ? (ptr + A) : (ptr + part_size * (i + 1));
    char buffer[G];
    int urandom;
    int j = G;
    while (1)
    {
        for (char *address = part_start; address < part_end; address++)
        {
            if (j == G)
            {
                urandom = open("/dev/urandom", O_RDONLY);
                read(urandom, buffer, G);
                close(urandom);
                j = 0;
            }
            *address = buffer[j++];
        }
    }
}

void *write_area_to_files(void *data)
{
    int thread_number = *(int *)data;
    int first_file_number = thread_number * 3;
    int current_file_number = thread_number * 3;
    int writes_number = E / ALIGNMENT_BLOCK_SIZE + (E % ALIGNMENT_BLOCK_SIZE ? 1 : 0);
    int writer_threads_number = I / 3 + (I % 3 ? 1 : 0);
    char *start_adress = ptr + A / writer_threads_number * thread_number;
    char *end_adress;
    if (thread_number == writer_threads_number)
    {
        end_adress = ptr + A;
    }
    else
    {
        end_adress = ptr + A / writer_threads_number * thread_number;
    }
    char *address = start_adress;
    while (1)
    {
        char file_name_buffer[8];
        snprintf(file_name_buffer, 8, "file_%d", current_file_number);
        int current_file;
        sem_wait(&semaphore);
        if ((current_file = open(file_name_buffer, O_WRONLY | O_CREAT | __O_DIRECT, 0644)) < 0)
        {
            printf("Failed to open file '%s' for writing\n", file_name_buffer);
            sem_post(&semaphore);
            continue;
        }
        else
        {
            printf("File '%s' was opened for writing\n", file_name_buffer);
        }

        for (int i = 0; i < writes_number; i++)
        {
            void *aligned_buffer;
            posix_memalign(&aligned_buffer, ALIGNMENT_BLOCK_SIZE, ALIGNMENT_BLOCK_SIZE);
            memcpy(aligned_buffer, address, G);
            if (write(current_file, aligned_buffer, ALIGNMENT_BLOCK_SIZE) == -1)
            {
                printf("Failed to write to file '%s'\n", file_name_buffer);
            }
            address += G;
            if (address + 123 > end_adress)
            {
                address = start_adress;
            }
        }
        printf("File '%s' was closed for writing\n", file_name_buffer);
        close(current_file);
        sem_post(&semaphore);
        current_file_number++;
        if (current_file_number == first_file_number + 3 || current_file_number == I)
        {
            current_file_number = first_file_number;
        }
    }
}

void *read_area_from_files(void *data)
{
    int i = *(int *)data;
    int count = 0;

    while (1)
    {
        char sum_value = 0;

        char file_name_buffer[8];
        snprintf(file_name_buffer, 8, "file_%d", i);
        int file;
        int j = 0;
        int bytes_read = 0;
        char buffer[G];
        sem_wait(&semaphore);
        if ((file = open(file_name_buffer, O_RDONLY)) < 0)
        {
            sem_post(&semaphore);
            continue;
        }
        else
        {
            printf("File '%s' was opened for reading\n", file_name_buffer);
        }
        while (1)
        {
            if (j == bytes_read)
            {

                bytes_read = read(file, buffer, G);
                j = 0;
                if (bytes_read == 0)
                {
                    break;
                }
            }
            count++;
            char current_value = buffer[j++];
            sum_value += current_value;
        }
        int avg_value = sum_value / count;
        close(file);
        sem_post(&semaphore);
    }
}

int main()
{
    sem_init(&semaphore, 0, 5);
    printf("Preallocation phase");
    getchar();
    ptr = mmap(B, A * sizeof(int), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    printf("Postallocation phase");
    getchar();
    pthread_t fill_threads[D];
    int fill_thread_numbers[D];
    for (int i = 0; i < D; i++)
    {
        fill_thread_numbers[i] = i;
        pthread_create(&fill_threads[i], NULL, fill_area, (void *)&fill_thread_numbers[i]);
    }
    printf("Memory filled phase finished");
    getchar();
    int writer_threads_number = I / 3 + (I % 3 ? 1 : 0);
    pthread_t write_threads[writer_threads_number];
    int write_thread_numbers[writer_threads_number];
    for (int i = 0; i < writer_threads_number; i++)
    {
        write_thread_numbers[i] = i;
        pthread_create(&write_threads[i], NULL, write_area_to_files, (void *)&write_thread_numbers[i]);
    }

    pthread_t read_threads[I];
    int read_thread_numbers[I];
    for (int i = 0; i < I; i++)
    {
        read_thread_numbers[i] = i;
        pthread_create(&read_threads[i], NULL, read_area_from_ files, (void *)&read_thread_numbers[i]);
    }

    pthread_join(write_threads[0], NULL);

    sem_destroy(&semaphore);

    printf("Predeallocation phase");
    getchar();

    munmap(ptr, A);

    printf("Postdeallocation phase");
    getchar();

    return 0;
}