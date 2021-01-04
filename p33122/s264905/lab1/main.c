#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>

// A=217;B=0xC7357F5A;C=malloc;D=36;E=87;F=block;G=147;H=seq;I=19;J=max;K=flock

#define MEM_MB_SIZE 217
#define THREAD_ALLOCATION_COUNT 36
#define FILE_MB_SIZE 87
#define IO_BLOCK_SIZE 147
#define THREAD_READING_COUNT 19

const int MEMORY_ALLOCATION_SIZE = MEM_MB_SIZE * 1000 * 1000;
const int FILES_COUNT = MEM_MB_SIZE % FILE_MB_SIZE == 0 ? MEM_MB_SIZE / FILE_MB_SIZE : MEM_MB_SIZE / FILE_MB_SIZE + 1;
FILE* random_file;
int max_number = 0;


typedef struct  {
    void *address;
    int mem_part_size;
} ThreadFillMemoryArgs;

typedef struct {
    int id;
    pthread_mutex_t *mut;
} ThreadReadFilesArgs;

void fillMemory(unsigned char *);
void writeMemoryToFiles(unsigned char *);
void* threadGenerateNumbers(void*);
void* threadReadFiles(void*);
void readFiles();

int main() {
    printf("-— Before allocation. Press ENTER to continue --\n");
    getchar();
    unsigned char *memory_numbers = malloc(MEMORY_ALLOCATION_SIZE);
    printf("-- After allocation. Press ENTER to generate numbers and fill memory --\n");
    getchar();
    do {
        printf("\n-- Next iteration --\n-- Starting filling memory --\nt");
        fillMemory(memory_numbers);
        writeMemoryToFiles(memory_numbers);
        readFiles();
    } while (1);
}

void fillMemory(unsigned char *memory_numbers) {
    random_file = fopen("/dev/urandom", "r");
    pthread_t threads[THREAD_ALLOCATION_COUNT];
    ThreadFillMemoryArgs threadFillMemoryArgs[THREAD_ALLOCATION_COUNT];
    ThreadFillMemoryArgs *ptr = threadFillMemoryArgs;
    int mem_fragment_size = MEMORY_ALLOCATION_SIZE / THREAD_ALLOCATION_COUNT;
    int last_mem_fragment_size = MEMORY_ALLOCATION_SIZE % THREAD_ALLOCATION_COUNT;


    // Потоки для заполнения памяти
    for (int i = 0; i < THREAD_ALLOCATION_COUNT; i++) {
        ptr[i].address = memory_numbers + mem_fragment_size * i;
        ptr[i].mem_part_size = i == (THREAD_ALLOCATION_COUNT - 1) ? last_mem_fragment_size : mem_fragment_size;
        pthread_create(&threads[i], NULL, threadGenerateNumbers, &ptr[i]);
    }

    for (int i = 0; i < THREAD_ALLOCATION_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }
    fclose(random_file);
    printf("\n\n-- After filling. Writing memory to files --\n");
}

void* threadGenerateNumbers(void* threadGeneratorArgs) {
    ThreadFillMemoryArgs *threadGeneratorArgs1 = (ThreadFillMemoryArgs*) threadGeneratorArgs;
    fread(threadGeneratorArgs1->address, 1, threadGeneratorArgs1->mem_part_size, random_file);
    return NULL;
}

void writeMemoryToFiles(unsigned char *memory_numbers) {
    int files_size = FILE_MB_SIZE * 1000 * 1000;
    unsigned char *ptr = memory_numbers;

    // Запись в файлы
    for (int i = 0; i < FILES_COUNT; i++) {
        char file_name[14];
        sprintf(file_name, "os_file_%d", i);
        FILE* out_file = fopen(file_name, "wb+");

        if (i != FILES_COUNT - 1) {
            int file_part_count = files_size / IO_BLOCK_SIZE;

            for (int j = 0; j < file_part_count; j++) {
                ptr += IO_BLOCK_SIZE;
                fwrite(ptr, 1, IO_BLOCK_SIZE, out_file);
            }
        } else {
            while (ptr <= (memory_numbers + MEMORY_ALLOCATION_SIZE)) {
                ptr += IO_BLOCK_SIZE;
                fwrite(ptr, 1, IO_BLOCK_SIZE, out_file);
            }
        }

        fclose(out_file);
        printf("\nFile %d was written\n", i);
    }
    printf("-- After writing. Reading files and finding max number --\n");
}

void readFiles() {
    pthread_t threads[THREAD_READING_COUNT];
    ThreadReadFilesArgs threadReadFilesArgs[THREAD_READING_COUNT];
    ThreadReadFilesArgs *ptr = threadReadFilesArgs;
    pthread_mutex_t *mutArr = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t) * FILES_COUNT);

    for (int i = 0; i < FILES_COUNT; i++) {
        pthread_mutex_init(&mutArr[i], NULL);
    }


    // Потоки для чтения файлов и поиска максимального числа
    for (int i = 0; i < THREAD_READING_COUNT; i++) {
        ptr[i].id = i;
        ptr[i].mut = mutArr;
        pthread_create(&threads[i], NULL, threadReadFiles, &ptr[i]);
    }

    for (int i = 0; i < THREAD_READING_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    free(mutArr);
    printf("\nMax number: %d\n", max_number);
}

void* threadReadFiles(void* threadReadFilesArgs) {
    int in_file;
    ThreadReadFilesArgs  *threadReadFilesArgs1 = (ThreadReadFilesArgs *) threadReadFilesArgs;

    for (int i = 0; i < FILES_COUNT; i++) {
        pthread_mutex_lock(&threadReadFilesArgs1->mut[i]);

        char file_name[10];
        sprintf(file_name, "os_file_%d", i);
        in_file = open(file_name, O_RDONLY);

        flock(in_file, LOCK_EX);

        struct stat st;
        stat(file_name, &st);

        unsigned char* buffer = (unsigned char*) malloc(IO_BLOCK_SIZE);

        for (int j = 0; j < st.st_size / IO_BLOCK_SIZE; j++) {
            read(in_file, buffer, IO_BLOCK_SIZE);
            if (buffer[0] > max_number) {
                max_number = buffer[0];
            }
        }

        flock(in_file, LOCK_UN);
        close(in_file);
        free(buffer);
        pthread_mutex_unlock(&threadReadFilesArgs1->mut[i]);
    }
    return NULL;
}
