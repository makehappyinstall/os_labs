#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <linux/futex.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/shm.h>
#include <limits.h>
#include <inttypes.h>
#include <sched.h>
#include <stdbool.h>

#define ALLOC_ADDR      0xB44603B3
#define THREADS_AMOUNT  3
#define FILL_MEM_SIZE   118
#define DUMP_FILE_SIZE  45
#define DATA_BLOCK_SIZE 11
#define COUNTERS_THREAD_AMOUNT 11
#define WORD_SIZE 8

struct threadFuncArg {
    void * writeAddr;
    int size;
    FILE * dataSource;
    int logEnabled;
};

void*
threadFunc(void *restrictArg)
{
    struct threadFuncArg * arg = (struct threadFuncArg*) restrictArg;

    fread(arg->writeAddr, 1, arg->size, arg->dataSource);
    if (arg->logEnabled) {
        fprintf(stderr, "Filling address: %p - pid: %u \t| tid: %lu\n", arg->writeAddr, getpid(), pthread_self());
    }

    pthread_exit((void *)0);
}

void
systemFree()
{
    system("free 1>&2");
    fprintf(stderr, "\n");
}

/*
 * Fills memory with mmap function.
 *
 * addr - pointer to fill address (may not equal to return pointer)
 * size - allocation size
 * logEnabled - log memory allocations (before and after allocation)
 */
void *
fmem(void *addr, size_t size, int logEnabled)
{

    int err;

    if (logEnabled) {
        fprintf(stderr, "\nBefore allocation:");
        systemFree();
    }

    void * mmapAddr = mmap(addr, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if (logEnabled) {
        fprintf(stderr, "\nAfter allocation: ");
        systemFree();
    }

    pthread_t threads [THREADS_AMOUNT] ;
    struct threadFuncArg * arg;

    for (size_t i = 0; i < THREADS_AMOUNT; i++) {

        arg = malloc(sizeof(arg));
        arg->size = size / THREADS_AMOUNT;
        arg->writeAddr = (uint8_t*)mmapAddr + i * ((arg->size / WORD_SIZE) + (arg->size % WORD_SIZE > 0 ? 1 : 0));
        arg->dataSource = fopen("/dev/urandom", "r");
        arg->logEnabled = logEnabled;
        err = pthread_create(&threads[i], NULL, threadFunc, arg);

        if (err != 0) {
            fprintf(stderr, "Error with creating thread");
            exit(err);
        }

    }

    for (size_t i = 0; i < THREADS_AMOUNT; i++) {
        err = pthread_join(threads[i], NULL);

        if (err != 0){
            exit(err);
        }
    }

    if (logEnabled) {
        fprintf(stderr, "\nAfter filling: ");
        systemFree();

        fprintf(stderr, "Compare mmap returned addr: %p and map addr: %p \n", mmapAddr, addr);
    }

    return mmapAddr;
}

int futex(int* uaddr, int futex_op, int val)
{
    return syscall(SYS_futex, uaddr, futex_op, val, NULL, NULL, 0);
}

void waitOnFutexValue(int* futex_addr, int val) {
    futex(futex_addr, FUTEX_WAIT, val);
}

void wakeFutexBlocking(int* futex_addr, int val) {
    while (1) {
        int futex_rc = futex(futex_addr, FUTEX_WAKE, val);
        if (futex_rc == -1) {
            perror("futex wake");
            exit(1);
        } else if (futex_rc > 0) {
            return;
        }
    }
}

/*
 * Dump memory to provided file descriptor
 */
void
dumpMem(const int fd, const void * addr, const int size, int * futex) {
    waitOnFutexValue(futex, 0);
    *futex = 0;
    fprintf(stderr, "Dump memory to %d\t from %p \t[size=%.2f MB]\n", fd, addr, size / (1024.0 * 1024.0));

    size_t iterations = size / DATA_BLOCK_SIZE;
    int tenPercent = iterations / 10;
    int res = ftruncate(fd, 0);
    if (res == -1) {
        fprintf(stderr, "truncate : %d\n", errno);
    }
    fprintf(stderr, "Truncate file result: %d\n", res);
    for (size_t i = 0; i < iterations; i++) {
        if (i % tenPercent == 0) {
            fprintf(stderr, "%ld0%% - %.2f MB\n", i / tenPercent, (size - i * DATA_BLOCK_SIZE) / (1024.0 * 1024.0));
        }
        void * pointer = (uint8_t*) addr + i;
        write(fd, &pointer, DATA_BLOCK_SIZE);
    }

    *futex = 1;
}

void
counter(unsigned int times) {
    for (size_t i = times; i > 0; i--) {
        fprintf(stderr, "...%ld\n", i);
        sleep(1);
    }
}

struct memoryDumpMap {
    int fd;
    void * futex;
    void * addr;
    int size;
};

struct dumpTFArgs {
    struct memoryDumpMap* dumpMap[10];
    int filesAmount;
};

void *
dumpMemThreadFunc(void * arg) {
    struct dumpTFArgs * dumpArg = (struct dumpTFArgs *) arg;
    while (1) {
        for (int i = 0; i < dumpArg->filesAmount; i++) {
            struct memoryDumpMap * dumpMap = dumpArg->dumpMap[i];
            dumpMem(dumpMap->fd, dumpMap->addr, dumpMap->size, dumpMap->futex);
            puts("");
            sleep(1);
        }
    }
}

int
aggregateFile(const struct memoryDumpMap * args) {
    int * futex = args->futex;
    waitOnFutexValue(futex, 0);
    *futex = 0;
    uint64_t sum = 0;

    if (lseek(args->fd, 0, SEEK_SET) == -1) {
        return -1;
    }

    unsigned char buf [DATA_BLOCK_SIZE];

    while(1) {

        size_t readBytes = read(args->fd, &buf, DATA_BLOCK_SIZE);

        if (readBytes == -1) {
            perror("Error file read");
            break;
        }

        if (readBytes < DATA_BLOCK_SIZE) {
            if (readBytes == 0) {
                break;
            }
        } else {
            for (int i = 0; i < DATA_BLOCK_SIZE; i++) {
                sum += buf[i];
            }
        }
    }

    fprintf(stderr, "Calculated sum on file [%d]:\t %lu\n", args->fd, sum);

    *futex = 1;

    return 0;
}

void *
readFileFunc(void * arg) {
    struct dumpTFArgs * dumpArg = (struct dumpTFArgs *) arg;
    while(1) {
        int index = rand() % dumpArg->filesAmount;
        aggregateFile(dumpArg->dumpMap[index]);
        sleep(1);
    }
}


int main()
{
    // Memory allocation operations

    const int fillMemSize = FILL_MEM_SIZE * 1024 * 1024;
    fprintf(stderr, "\nAllocating memory with mmap function");
    void * mmapAddr = fmem((void *)ALLOC_ADDR, fillMemSize, true);

    munmap(mmapAddr, fillMemSize);

    // just for in-app monitor
    // of course, we can make an additional monitor outside
    fprintf(stderr, "\nAfter deallocation");
    systemFree();



    // Dump memory to files and aggregate them in separate threads with futex

    const void * memoryAddr = fmem((void *) ALLOC_ADDR, FILL_MEM_SIZE, false);

    const size_t dumpMemSize = DUMP_FILE_SIZE * 1024 * 1024;
    const size_t filesAmount = fillMemSize / dumpMemSize + (fillMemSize % dumpMemSize > 0 ? 1 : 0);

    int files[filesAmount];

    struct dumpTFArgs * args = malloc(sizeof(struct dumpTFArgs));
    args->filesAmount = filesAmount;

    for (size_t i = 0; i < filesAmount; i++) {
        const size_t filenameSize = 6 + log10(filesAmount) + 1;
        char filename[filenameSize];
        snprintf(filename, filenameSize, "dump.%ld", i);

        files[i] = open(filename, O_RDWR | O_CREAT, (mode_t) 0600);

        if (files[i] == -1) {
            fprintf(stderr, "Error: Can't create or open file");
        }

        args->dumpMap[i] = malloc(sizeof(struct memoryDumpMap));
        args->dumpMap[i]->fd = files[i];
        args->dumpMap[i]->addr = (uint8_t*) memoryAddr + i * (dumpMemSize / WORD_SIZE);
        args->dumpMap[i]->size = dumpMemSize;

        const int shm_id = shmget(IPC_PRIVATE, 4096, IPC_CREAT | 0666);
        args->dumpMap[i]->futex = shmat(shm_id, NULL, 0);
    }


    pthread_t memDumpTP;
    pthread_attr_t * memDTA = malloc(sizeof(pthread_attr_t));
    struct sched_param * memDP = malloc(sizeof(struct sched_param));


    pthread_attr_setschedparam(memDTA, memDP);
    pthread_create(&memDumpTP, NULL, dumpMemThreadFunc, args);

    fprintf(stderr, "Write-thread created...\n");

    fprintf(stderr, "Creating aggregator-threads\n");

    pthread_t counters[COUNTERS_THREAD_AMOUNT];

    pthread_attr_t * memAttr = malloc(sizeof(pthread_attr_t));
    struct sched_param * memSP = malloc(sizeof(struct sched_param));
    pthread_attr_setschedparam(memAttr, memSP);

    for (size_t i = 0; i < COUNTERS_THREAD_AMOUNT; i++) {
        counters[i] = pthread_create(&counters[i], NULL, readFileFunc, args);
    }

    fprintf(stderr, "Created %d aggregator-threads\n", COUNTERS_THREAD_AMOUNT);

    const int times = 3;
    fprintf(stderr, "Unlocking all blocks after %d seconds\n", times);
    counter(times);


    fprintf(stderr, "Unlocking all blocks...\n");

    for (size_t i = 0; i < filesAmount; i++) {
        int * futex = args->dumpMap[i]->futex;
        wakeFutexBlocking(futex, 1);
    }

    int err = pthread_join(memDumpTP, NULL);
    if (err != 0) {
        fprintf(stderr, "Error pthred_join. Status: %d\n", err);
    }

    for (size_t i = 0; i < COUNTERS_THREAD_AMOUNT; i++) {
        pthread_join(counters[i], NULL);
    }

    return 0;
}
