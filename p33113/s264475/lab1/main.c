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
#define RANDOM_SIZE 5000
#define E_OUTPUT_SIZE 55
#define G_BLOCK_SIZE 108
#define I_AGG_THREADS 135


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

int randomByteIndex;
char randomChar[RANDOM_SIZE];

unsigned char ReadChar(int randomFD) {
    if (randomByteIndex >= RANDOM_SIZE) {
        size_t result = read(randomFD, &randomChar, sizeof(randomChar));
        if (result == -1) {
            perror("Ошибка с /dev/urandom\n");
            return 0;
        }
        randomByteIndex = 0;
    }
    return randomChar[randomByteIndex++];
}

void *WriteToMemory(void *args) {
    MemoryWriterData *writeArgs = (MemoryWriterData *) (struct WriteToMemoryArgs *) args;
    unsigned int bytesWrote = 0;
    for (int i = writeArgs->start; i < writeArgs->end; ++i) {
        unsigned char random = ReadChar(writeArgs->randomFD);
        writeArgs->address[i] = random;
        bytesWrote += 1;
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
    struct stat fstat;
    /* Get file attributes for FILE and put them in BUF.  */
    stat(filepath, &fstat);
    int blksize = (int) fstat.st_blksize;
    int align = blksize - 1;
    int bytes = size * n;
//    невозможно использовать G, потому что флаг O_DIRECT требует,
//    чтобы адрес памяти и буфер были согласованы с размером
//    блока файловой системы.
//  SOLUTION: https://coderoad.ru/34182535/%D0%9E%D1%88%D0%B8%D0%B1%D0%BA%D0%B0-%D0%B7%D0%B0%D0%BF%D0%B8%D1%81%D0%B8-%D0%BD%D0%B5%D0%B4%D0%BE%D0%BF%D1%83%D1%81%D1%82%D0%B8%D0%BC%D1%8B%D0%B9-%D0%B0%D1%80%D0%B3%D1%83%D0%BC%D0%B5%D0%BD%D1%82-%D0%BA%D0%BE%D0%B3%D0%B4%D0%B0-%D1%84%D0%B0%D0%B9%D0%BB-%D0%BE%D1%82%D0%BA%D1%80%D1%8B%D0%B2%D0%B0%D0%B5%D1%82%D1%81%D1%8F-%D1%81-%D0%BF%D0%BE%D0%BC%D0%BE%D1%89%D1%8C%D1%8E-O_DIRECT
    int blocks = bytes / blksize;

    char *buff = (char *) malloc(blksize + align);
    // из SOLUTION
    char *wbuff = (char *) (((uintptr_t) buff + align) & ~((uintptr_t) align));

    for (int i = 0; i < blocks; ++i) {
        char *buf_ptr = (char *) ptr + blksize * i;
        // копируем из памяти для записи в буфер
        for (int j = 0; j < blksize; j++) {
            buff[j] = buf_ptr[j];
        }
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

    char filename[6] = "lab1_0";
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
    for (int i = 0; i < file_size / 4; i++) {
        sum += int_buf[i];
    }

    free(buffer);
    return NULL;
}


int main() {

    int mBytes = A_DATA_SIZE * pow(10, 6); // size in megabytes
    unsigned char *address = (unsigned char *) B_ADRRESS; // starting address


    int outputFD = open("output", O_RDWR | O_TRUNC | O_CREAT, (mode_t) 0777);
    if (outputFD < 0) {
        perror("Невозможно открыть файл.\n");
        return 1;
    }

    int randomFD = open("/dev/urandom", O_RDONLY);
    if (randomFD < 0) {
        perror("Невозможно прочитать /dev/urandom.\n");
        return 1;
    }


    if (lseek(outputFD, mBytes, SEEK_SET) < 0) {
        close(outputFD);
        perror("Ошибка во время вызова lseek() - для проверки доступности.");
        return 1;
    }

    if (write(outputFD, "", 1) < 0) {
        close(outputFD);
        perror("Ошибка во время записи последнего байта в файл");
        return 1;
    }

    char ch;
    printf("Для старта нажмите любую кнопку.");
    while ((ch = getchar()) != '\n' && ch != EOF);
    printf("После ввода символа программа аллоцирует память\n");
    getchar();


    int *map_ptr = mmap(address, mBytes,
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED,
                        outputFD, 0);
    if (map_ptr == MAP_FAILED) {
        perror("Ошибка при маппинге\n");
        return 1;
    }
    while ((ch = getchar()) != '\n' && ch != EOF);
    printf("После ввода символа программа будет записывать данные в память\n");
    getchar();

//    ================================
//    WRITE DATA TO MEMORY
//    ================================

    const int mem_part = mBytes / D_THREADS / sizeof(*address);
    const int mem_last = mBytes % D_THREADS / sizeof(*address);

//    https://habr.com/ru/post/326138/
    pthread_t writeToMemoryThreads[D_THREADS];
    MemoryWriterData *args;
    int i;

    printf("Запись случайных данных в память...\n");
    struct timespec start, finish;
    double elapsed;
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

    while ((ch = getchar()) != '\n' && ch != EOF);
    printf("После ввода символа программа будет записывать данные в файл\n");
    getchar();


//    ================================
//    WRITES DATA TO FILES
//    ================================


    printf("Запись случайных данных в файл...\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    int filesAmount = (A_DATA_SIZE / E_OUTPUT_SIZE) + 1;
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

//    ================================
//    AGGREGATE DATA
//    ================================

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


    if (munmap(map_ptr, mBytes) == -1) {
//        close(outputFD);
        close(randomFD);
        perror("Ошибка при размапивании Лехи.");
        exit(EXIT_FAILURE);
    }

    while ((ch = getchar()) != '\n' && ch != EOF);
    printf("После ввода символа программа будет записывать данные в файл\n");
    getchar();


    if (close(outputFD)) {
        printf("Ошибка во время закрытия файла.\n");
    }

    if (close(randomFD)) {
        printf("Ошибка во время закрытия файла.\n");
    }


    return 0;


}
