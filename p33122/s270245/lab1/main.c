#include <stdio.h>
#include <sys/mman.h>
#include <stdint.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <errno.h>
#include <stdlib.h>
#include <sched.h>
#include <signal.h>
#include <wait.h>

const size_t A = 101;
const uintptr_t B = 0xD9FE0449;
const int D = 121;
const int E = 129;
const int G = 39;
const int I = 99;

size_t memorySize = A * 1024 * 1024;
size_t fileSize = E * 1024 * 1024;
size_t bufferSize = G * 128; // Used a larger IO buffer because this takes way too long otherwise
char savePath[] = "mem";

unsigned char* memory;
FILE *randomDataProvider;
int isReaderProcessFinished = 0;


struct portion
{
    uintptr_t start;
    size_t size;
    unsigned int result;
};



void allocate()
{
    memory = mmap((void*) B, memorySize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_FILE |MAP_PRIVATE, -1, 0);

    if (memory == MAP_FAILED)
    {
        perror("Error allocating memory");
        exit(errno);
    }
}



void *fillPortion(void * params)
{
    struct portion *inf = (struct portion *) params;

    size_t readResult = fread(memory + inf->start, inf->size, 1, randomDataProvider);
    if (readResult < 0)
    {
        //printf("%zu %zu\n", inf->start, inf->size);
        perror("Error filling memory with junk");
        exit(errno);
    }

    return NULL;
}

void fill()
{
    size_t dataPerThread = memorySize / D;
    pthread_t threads[D];

    randomDataProvider = fopen("/dev/urandom\0", "r");

    for (int i = 0; i < D; i++)
    {
        struct portion p;
        p.start = i * dataPerThread;
        p.size = dataPerThread;

        pthread_create(&threads[i], NULL, fillPortion, &p);
    }
    for (int i = 0; i < D; i++)
    {
        pthread_join(threads[i], NULL);
    }

    fclose(randomDataProvider);
}



size_t min(size_t a, size_t b)
{
    return a < b ? a : b;
}

void saveToFile()
{
    int f = open(savePath, O_CREAT | O_WRONLY,  S_IRUSR | S_IWUSR);
    flock(f, LOCK_EX);

    size_t written = 0;
    int writeResult;
    while (written < fileSize)
    {
        //printf("%zu / %zu\n", written, fileSize);
        writeResult = write(f, memory + (written % memorySize),            // Wrapping data for the extra space
                            min(bufferSize, memorySize - (written % memorySize)));  // Do not go over the memory

        if (writeResult < 0)
        {
            perror("Error writing to a file");
            exit(errno);
        }
        written += writeResult;
    }

    flock(f, LOCK_UN);
    close(f);
}



void *readPortion(void * params)
{
    struct portion *inf = (struct portion *) params;
    FILE* file = fopen(savePath, "r");

    flock(fileno(file), LOCK_EX);

    unsigned int max = 0;

    size_t read = 0;
    while (read < inf->size)
    {
        unsigned int num;
        fseek(file, inf->start, SEEK_SET);
        if (fread(&num, sizeof(unsigned int), 1, file) < 0)
        {
            perror("Error reading from memory");
            exit(errno);
        }
        max = num > max ? num : max;
        read += sizeof(unsigned int);
    }

    inf->result = max;

    flock(fileno(file), LOCK_UN);
    fclose(file);

    return NULL;
}

void *readFromFile(void * params)
{
    while (1)
    {
        printf("Press Enter to start reading from a file, or q to quit ");
        int input = getchar();
        if (input == 'q')
            break;


        size_t dataPerThread = fileSize / I;
        pthread_t threads[I];
        struct portion portions[I];

        for (int i = 0; i < I; i++)
        {
            portions[i].start = i * dataPerThread;
            portions[i].size = dataPerThread;

            pthread_create(&threads[i], NULL, readPortion, &portions[i]);
        }

        unsigned int max = 0;
        for (int i = 0; i < I; i++)
        {
            pthread_join(threads[i], NULL);
            printf("Thread %u - max is %u\n", i + 1, portions[i].result);
            max = portions[i].result > max ? portions[i].result : max;
        }

        printf("Total max is %u\n", max);
    }

    isReaderProcessFinished = 1;

    return NULL;
}

void startReadingProcess()
{
    int stackSize = 1024*1024;
    void *stack = (void **) malloc(stackSize);
    if (stack == MAP_FAILED)
    {
        perror("Error allocating memory for a reading process");
        exit(errno);
    }

    pthread_t p;
    pthread_create(&p, NULL, readFromFile, NULL);
}



void deallocate()
{
    if (munmap(memory, memorySize) < 0)
    {
        perror("Error deallocating memory");
        exit(errno);
    }
}



void debugPause(char *message)
{
    printf("%s\n", message);
    getchar();
}

int main(int argc, char* argv[])
{
    if (argc > 1) // if there are any arguments
    {
        debugPause("Program started in memory measurement mode");
        allocate();
        debugPause("Memory has been allocated");
        fill();
        debugPause("Memory has been filled");
        deallocate();
        debugPause("Memory has been deallocated");
    }
    else
    {
        allocate();
        startReadingProcess();
        while (!isReaderProcessFinished)
        {
            fill();
            saveToFile();
        }
        deallocate();
    }
}
