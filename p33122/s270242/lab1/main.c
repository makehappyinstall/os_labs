#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <limits.h>
#include <pthread.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>

//variant: A=213;B=0xD9B51893;C=malloc;D=62;E=177;F=block;G=21;H=seq;I=40;J=max;K=cv
#define A 213
#define D 62
#define E 177
#define G 21
#define I 40

pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
const size_t MEMORY_SIZE = A * 1024 * 1024;
const size_t FILE_SIZE = E * 1024 * 1024;
const size_t BLOCK_SIZE = G * sizeof(char);

//2 step
void *file_writer(void *start_address)
{
    while(1)
    {
        printf("Thread in delay on write...\n");
        pthread_mutex_lock(&mutex);

        printf("Thread lock the mutex on write\n");
        FILE *file_write = fopen("target_file", "w");

        if (file_write != NULL)
            fwrite(start_address, BLOCK_SIZE, FILE_SIZE / BLOCK_SIZE, file_write);
        else
            fprintf(stderr, "FileNotFoundError");

        fclose(file_write);

        pthread_cond_broadcast(&cv);
        pthread_mutex_unlock(&mutex);
        printf("Thread unlock the mutex on write\n");
    }
}

//step 3
void *file_reader()
{
    while(1) {
        printf("Thread in delay on read...\n");
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&cv, &mutex);
        printf("Thread lock the mutex on read\n");

        int fd_read = open("target_file", O_RDONLY);
        int mx = INT_MIN;
        int value;

        if (fd_read == -1)
            fprintf(stderr, "FileNotFoundError");

        while (read(fd_read, &value, sizeof(int)) >= sizeof(int)) {
            read(fd_read, &value, sizeof(int));

            if (mx < value) mx = value;
        }

        close(fd_read);

        printf("Result: %d\n", mx);
        pthread_mutex_unlock(&mutex);
        printf("Thread unlock the mutex on read\n");
    }
}

int main() {
    //1 step
//    printf("Before allocation\n");
//    getchar();
    void *start_address = malloc(MEMORY_SIZE + 1);

//        printf("After allocation\n");
//        getchar();
    pthread_t threads[D];
    FILE *file_urand;
    file_urand = fopen("/dev/urandom", "r");

    void* fill_memory() { fread(start_address, 1, MEMORY_SIZE, file_urand); }

    if(file_urand != NULL)
    {
        for(int i = 0; i < D; i++)
            pthread_create(&threads[i], NULL, fill_memory, NULL);

        for(int i = 0; i < D; i++)
            pthread_join(threads[i], NULL);
    }
    else fprintf(stderr, "FileNotFoundError");

    fclose(file_urand);
    //printf("After memory filling\n");
    //    getchar();

        //step 4
    pthread_t fill_thread;
    pthread_t aggregate_threads[I];

    pthread_create(&fill_thread, NULL, file_writer, (void *)start_address);

    for (int i = 0; i < I; i++)
        pthread_create(&aggregate_threads[i], NULL, file_reader, NULL);


    pthread_join(fill_thread, NULL);

    for (int i = 0; i < I; i++)
        pthread_join(aggregate_threads[i], NULL);


    free(start_address);

//    printf("After deallocation\n");
    //getchar();

    return 0;
}
