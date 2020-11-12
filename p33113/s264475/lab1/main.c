#include <stdio.h>
#include <sys/mman.h>
#include <math.h>
#include <stdlib.h>
#include <zconf.h>
#include <pthread.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>


#define A_DATA_SIZE 313
#define B_ADRRESS 0xE46AE4C0
#define D_THREADS 89
//#define D_THREADS 10
#define RANDOM_SIZE 5000
#define E_OUTPUT_SIZE 55
#define G_BLOCK_SIZE 108
#define I_AGG_THREADS 135
//#define I_AGG_THREADS 10


typedef struct {
    int randomFD;
    unsigned char *address;
    int mBytes;
    int start;
    int end;
    pthread_t threadId;
} MemoryWriterData;


typedef struct {
    int ints_per_file;
    int filesAmount;
    int *start;
    int *end;
} ThreadWriterData;


typedef struct {
    int thread_number;
    int file_number;
} ThreadReaderData;


int mBytes; // size in megabytes
unsigned char *address; // starting address
int outputFD;
int randomFD;
char ch;
int *map_ptr;
struct timespec start, finish;
double elapsed;
int filesAmount;





//   todo ОДЫН: Кажется что читать по одному байту за раз - так себе идея

//    todo ДВА: Кажется что у вас тут доступ из нескольких тредов к переменной.
//     Надо или сделать локальными (что лучше), или обложить синхронизацией
//     randomByteIndex.

void *WriteToMemory(void *args) {
    int randomByteIndex = RANDOM_SIZE + 1;
    char randomChar[RANDOM_SIZE];

    MemoryWriterData *writeArgs = (MemoryWriterData *) args;
    for (int i = writeArgs->start; i < writeArgs->end; ++i) {
        if (randomByteIndex >= RANDOM_SIZE) {
            size_t result = read(writeArgs->randomFD, &randomChar, sizeof(randomChar));
            if (result == -1) {
                perror("Ошибка с /dev/urandom\n");
                return 0;
            }
            randomByteIndex = 0;
        }
        writeArgs->address[i] = randomChar[randomByteIndex++];
    }

    free(writeArgs);
    return NULL;
}


char *seqRead(int fd, int file_size) {
    char *buffer = (char *) malloc(file_size);
    int blocks = file_size / G_BLOCK_SIZE;
    int last_block_size = file_size % G_BLOCK_SIZE;
    char *buf_ptr;
    for (int i = 0; i < blocks; ++i) {
        buf_ptr = buffer + G_BLOCK_SIZE * i;
        pread(fd, buf_ptr, G_BLOCK_SIZE, G_BLOCK_SIZE * i);
    }
    if (last_block_size > 0) {
        buf_ptr = buffer + G_BLOCK_SIZE * blocks;
        pread(fd, buf_ptr, last_block_size, G_BLOCK_SIZE * blocks);
    }
    return buffer;
}

void seqWrite(void *ptr, int size, int n, int fd, const char *filepath) {
//    невозможно использовать G, потому что флаг O_DIRECT требует,
//    чтобы адрес памяти и буфер были согласованы с размером
//    блока файловой системы.
    struct stat fstat;
    stat(filepath, &fstat);
    int blksize = (int) fstat.st_blksize; // file's block size
    int align = blksize - 1; // allocate align more bytes than I need.

    char *buff = (char *) malloc(blksize + align); // now we have enough memory
    char *wbuff = (char *) (((uintptr_t) buff + align) & ~((uintptr_t) align)); // "cutting off" the excess (using XOR)



    int bytes = size * n;
    int blocks = bytes / blksize;


    for (int i = 0; i < blocks; ++i) {
        char *buf_ptr = (char *) ptr + blksize * i;
        // копируем из памяти для записи в буфер
//        for (int j = 0; j < blksize; j++) {
//            buff[j] = buf_ptr[j];
//        }
        memcpy(buff, buf_ptr, blksize);
        if (pwrite(fd, wbuff, blksize, blksize * i) < 0) {
            free((char *) buff);
            printf("write error occurred\n");
            return;
        }
    }
    free((char *) buff);
}

void *writeToFiles(void *thread_data) {
    ThreadWriterData *data = (ThreadWriterData *) thread_data;
    int *write_pointer = data->start;

    for (int i = 0; i < data->filesAmount; i++) {
        char filename[9] = "os_file_0";
        filename[8] = '0' + i;

        struct flock readLock;
        memset(&readLock, 0, sizeof(readLock));

        // NOCACHE file write
        int current_file_fd = open(filename, O_WRONLY | O_CREAT | __O_DIRECT, 00666);

        readLock.l_type = F_RDLCK;
        fcntl(current_file_fd, F_SETLKW, &readLock);

        if (current_file_fd == -1) {
            printf("ошибка открытия файла на запись\n");
            return NULL;
        }
        int ints_to_file = data->ints_per_file;
        int is_done = 0;
        while (!is_done) {
            if (ints_to_file + write_pointer < data->end) {
                seqWrite(write_pointer, sizeof(int), ints_to_file, current_file_fd, filename);
                write_pointer += ints_to_file;
                is_done = 1;
            } else {
                int available = data->end - write_pointer;
                seqWrite(write_pointer, sizeof(int), available, current_file_fd, filename);
                write_pointer = data->start;
                ints_to_file -= available;
            }
        }
        readLock.l_type = F_UNLCK;
        fcntl(current_file_fd, F_SETLKW, &readLock);
        close(current_file_fd);

    }
    return NULL;
}


void *readFiles(void *thread_data) {
    ThreadReaderData *data = (ThreadReaderData *) thread_data;

    char filename[] = "lab1_0";
    filename[5] = '0' + data->file_number;
    int file_desc = -1;

    struct flock writeLock;
    memset(&writeLock, 0, sizeof(writeLock));
    while (file_desc < 0) {
        file_desc = open(filename, O_RDONLY, 00666);
    }
    writeLock.l_type = F_WRLCK;
    fcntl(file_desc, F_SETLKW, &writeLock);

    struct stat st;
    stat(filename, &st);
    int file_size = st.st_size;

    char *buffer = seqRead(file_desc, file_size);

    writeLock.l_type = F_UNLCK;
    fcntl(file_desc, F_SETLKW, &writeLock);
    close(file_desc);

    int *int_buf = (int *) buffer;
    long sum = 0;
    for (int i = 0; i < file_size / sizeof(int); i++) {
        sum += int_buf[i];
    }

    free(buffer);
    return NULL;
}


void init() {
    mBytes = A_DATA_SIZE * pow(10, 6); // size in megabytes
    address = (unsigned char *) B_ADRRESS; // starting address


    outputFD = open("output", O_RDWR | O_TRUNC | O_CREAT, (mode_t) 0777);
    if (outputFD < 0) {
        perror("Невозможно открыть файл.\n");
        exit(1);
    }

    randomFD = open("/dev/urandom", O_RDONLY);
    if (randomFD < 0) {
        perror("Невозможно прочитать /dev/urandom.\n");
        exit(1);
    }


    if (lseek(outputFD, mBytes, SEEK_SET) < 0) {
        close(outputFD);
        perror("Ошибка во время вызова lseek() - для проверки доступности.");
        exit(1);
    }

    if (write(outputFD, "", 1) < 0) {
        close(outputFD);
        perror("Ошибка во время записи последнего байта в файл");
        exit(1);
    }

    printf("Для старта нажмите любую кнопку.");
    while ((ch = getchar()) != '\n' && ch != EOF);
    printf("После ввода символа программа аллоцирует память\n");
    getchar();


    map_ptr = mmap(address, mBytes,
                   PROT_READ | PROT_WRITE,
                   MAP_SHARED,
                   outputFD, 0);
    if (map_ptr == MAP_FAILED) {
        perror("Ошибка при маппинге\n");
        exit(1);
    }

}

void writeDataToMemory() {
    while ((ch = getchar()) != '\n' && ch != EOF);
    printf("После ввода символа программа будет записывать данные в память\n");
    getchar();

    const int mem_part = mBytes / D_THREADS / sizeof(*address);
    const int mem_last = mBytes % D_THREADS / sizeof(*address);

    //    https://habr.com/ru/post/326138/
    pthread_t writeToMemoryThreads[D_THREADS];
    MemoryWriterData *args;
    int i;

    printf("Запись случайных данных в память...\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (i = 0; i < D_THREADS; ++i) {
        args = malloc(sizeof(*args));
        args->address = address;
        args->randomFD = randomFD;
        args->mBytes = mBytes;

        args->start = i * mem_part;
        args->end = (i + 1) * mem_part;
        args->threadId = writeToMemoryThreads[i];

        if (pthread_create(&writeToMemoryThreads[i], NULL, WriteToMemory, (void *) args)) {
            free(args);
            perror("Невозможно создать тред");
        }
//        printf("%d %d\n", i, &(address)[i]);

    }


    if (mem_last > 0) {
        args = malloc(sizeof(*args));
        args->address = address;
        args->randomFD = randomFD;
        args->mBytes = mBytes;

        args->start = i * mem_part;
        args->end = i * mem_part + mem_last;
        WriteToMemory((void *) args);
    }

    for (i = 0; i < D_THREADS; ++i) {
        pthread_join(writeToMemoryThreads[i], NULL);
    }


    clock_gettime(CLOCK_MONOTONIC, &finish);
    elapsed = (double) (finish.tv_sec - start.tv_sec);
    elapsed += (double) (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
    printf("Запись в память заняла %f секунл\n", elapsed);
}

void writeDataToFiles() {
    while ((ch = getchar()) != '\n' && ch != EOF);
    printf("После ввода символа программа будет записывать данные в файл\n");
    getchar();

    printf("Запись случайных данных в файл...\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    filesAmount = (A_DATA_SIZE / E_OUTPUT_SIZE) + 1;
    if (A_DATA_SIZE % E_OUTPUT_SIZE != 0) {
        filesAmount++;
    }

    printf("Число файлов: %d\n", filesAmount);


    ThreadWriterData *writer_data = (ThreadWriterData *) malloc(sizeof(ThreadWriterData));
    pthread_t *thread_writer = (pthread_t *) malloc(sizeof(pthread_t));
    writer_data->ints_per_file = E_OUTPUT_SIZE * 1024 * 256;
    writer_data->filesAmount = filesAmount;
    writer_data->start = map_ptr;
    writer_data->end = map_ptr + mBytes / 4;

    pthread_create(thread_writer, NULL, writeToFiles, writer_data);
    pthread_join(*thread_writer, NULL);


    clock_gettime(CLOCK_MONOTONIC, &finish);
    elapsed = (double) (finish.tv_sec - start.tv_sec);
    elapsed += (double) (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
    printf("Запись в файл заняла %f секунд\n", elapsed);
}

void aggregateData() {
    int i;
    printf("Подсчет аггрегирующей функции.\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    pthread_t *reader_threads = (pthread_t *) malloc(I_AGG_THREADS * sizeof(pthread_t));
    ThreadReaderData *reader_data = (ThreadReaderData *) malloc(I_AGG_THREADS * sizeof(ThreadReaderData));
    int file_number = 0;
    for (i = 0; i < I_AGG_THREADS; ++i) {
        if (file_number >= filesAmount) {
            file_number = 0;
        }
        reader_data[i].thread_number = i;
        reader_data[i].file_number = file_number;
        file_number++;
    }

    for (i = 0; i < I_AGG_THREADS; ++i) {
        pthread_create(&(reader_threads[i]), NULL, readFiles, &reader_data[i]);
    }
    for (i = 0; i < I_AGG_THREADS; i++) {
        pthread_join(reader_threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &finish);
    elapsed = (double) (finish.tv_sec - start.tv_sec);
    elapsed += (double) (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
    printf("Аггрегирование заняло %f секунд\n", elapsed);
}

void outro() {
    if (munmap(map_ptr, mBytes) == -1) {
//        close(outputFD);
        close(randomFD);
        perror("Ошибка при размапивании.");
        exit(EXIT_FAILURE);
    }


    if (close(outputFD)) {
        printf("Ошибка во время закрытия файла.\n");
    }

    if (close(randomFD)) {
        printf("Ошибка во время закрытия файла.\n");
    }
}


int main() {

    init();

//    ================================
//    WRITE DATA TO MEMORY
//    ================================
    writeDataToMemory();

//    ================================
//    WRITES DATA TO FILES
//    ================================

    writeDataToFiles();

//    ================================
//    AGGREGATE DATA
//    ================================

    aggregateData();

    outro();

    return 0;


}
