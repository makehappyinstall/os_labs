#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <limits.h>
#include <pthread.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>


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
const size_t MEMORY_SLICE = MEMORY_SIZE / D;
FILE *file_urand;

//2 step
void *file_writer(void *start_address)
{
    while(1)
    {
        printf("Thread in delay on write...\n");
        pthread_mutex_lock(&mutex);
        //pthread_cond_wait(&cv, &mutex);
        printf("Thread lock the mutex on write\n");
        FILE *file_write = fopen("target_file", "w");

        if (file_write != NULL)
            fwrite(start_address, G, FILE_SIZE / G, file_write);
        else {
            fprintf(stderr, "FileNotFoundError");
            return NULL;
        }

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
        int value[G];
        int block = G * 4;

        if (fd_read == -1)
        {
            fprintf(stderr, "FileNotFoundError");
            return NULL;
        }

        while (read(fd_read, &value, block) >= block) {

            for (int i = 0; i < G; i++) {
                if (mx < value[i]) mx = value[i];
            }
        }

        close(fd_read);

        printf("Result: %d\n", mx);
        //pthread_cond_broadcast(&cv);
        pthread_mutex_unlock(&mutex);
        printf("Thread unlock the mutex on read\n");
    }
}

void* fill_memory(void *address)
{
    pthread_mutex_lock(&mutex);

    fread(address, MEMORY_SLICE, 1, file_urand);

    pthread_mutex_unlock(&mutex);
}

int main() {
    //1 step
//    printf("Before allocation\n");
//    getchar();
    pthread_t threads[D];
    void *main_address = malloc(MEMORY_SIZE + 1);
    file_urand = fopen("/dev/urandom", "r");

//    printf("After allocation\n");
//    getchar();

    if(file_urand == NULL)
    {
        fprintf(stderr, "FileNotFoundError");
        return 0;
    }

    void *void_address;
    void_address = main_address;

    for(int i = 0; i < D; i++)
    {
        pthread_create(&threads[i], NULL, fill_memory, (void *) void_address);
        void_address += MEMORY_SLICE;
    }

    for(int i = 0; i < D; i++)
        pthread_join(threads[i], NULL);

    //printf("After memory filling\n");
    //    getchar();

        //step 4
    pthread_t fill_thread;
    pthread_t aggregate_threads[I];

    pthread_create(&fill_thread, NULL, file_writer, (void *) main_address);

    for (int i = 0; i < I; i++)
        pthread_create(&aggregate_threads[i], NULL, file_reader, NULL);

    pthread_join(fill_thread, NULL);



    for (int i = 0; i < I; i++)
        pthread_join(aggregate_threads[i], NULL);


    fclose(file_urand);
    free(main_address);

//    printf("After deallocation\n");
    //getchar();

    return 0;
}
