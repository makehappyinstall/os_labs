/**
 * Вариант:
 * A=352;B=0x81176CF6;C=mmap;D=69;E=18;F=nocache;G=62;H=seq;I=37;J=min;K=flock
 **/
#define _GNU_SOURCE
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>

#define MMAP_ADDRESS (void*) 0x81176CF6
#define MMAP_LENGTH 352000000 // 352 MB
#define MMAP_PROTECTION (PROT_READ | PROT_WRITE)
#define MMAP_FLAGS (MAP_PRIVATE | MAP_ANONYMOUS)
#define MMAP_FD -1 // flags = MAP_ANONYMOUS, but some systems require this to be -1
#define MMAP_OFFSET 0

#define FILE_SIZE 17999872 // 18 MB
#define RANDOM_SRC "/dev/urandom"
#define FILL_THREADS 69 
#define FILES_NUMBER (MMAP_LENGTH/FILE_SIZE + (MMAP_LENGTH % FILE_SIZE == 0 ? 0 : 1))
#define BLOCK_SIZE 512
#define ANALYZING_THREADS 37

#define FILE_LOCK_READ 0
#define FILE_LOCK_WRITE 1


void* fillThreadHandler(void*);

void generateData();
void writeData();
void writeSingleFile(char*, void*, int);
void freeData();
void startBackgroundReading();
void* fileAnalyzeThreadHandler(void*);
void setMinPriorityForCurrentThread();
void analyzeFile(char*);

void fileLock(int, int);
void fileUnlock(int, int);

void signalHandler(int);

void printCat();

void* regionPtr; // Указатель на область в памяти, которую будем данными заполнять
int randomFd; // Дескриптор файла /dev/urandom

int main() {
    // Сообщаем операционной системе, что мы хотим ловить сигналы SIGINT (нажатие на Ctrl+C) и SIVSEGV (segmentation violation signal, 'segmentation fault')
    // При поступлении сигнала будет вызвана функция signalHandler
    signal(SIGINT, signalHandler);
    signal(SIGSEGV, signalHandler);

    printCat();
    startBackgroundReading();

    while(1) {
        generateData();
        writeData();
        freeData();
    }
}

void generateData() {
    printf("This is where everything begins...\n");
    regionPtr = mmap(MMAP_ADDRESS, MMAP_LENGTH, MMAP_PROTECTION, MMAP_FLAGS, MMAP_FD, MMAP_OFFSET);

    if (regionPtr == MAP_FAILED) {
        printf("Не получилось сделать malloc, errno = %d\n", errno);
        exit(-1);
    } else {
        printf("mmap is done successfully, region created at %p\n", regionPtr);
    }
    printf("Заполнение случайными числами в %d потоков\n", FILL_THREADS);

    randomFd = open(RANDOM_SRC, O_RDONLY); // Открываем /dev/urandom для чтения
    if (randomFd < 0) { // Если randomFd < 0, значит что-то пошло не так
        printf("Опаньки...\n");
        printf("Не получилось открыть %s, errno = %d\n", RANDOM_SRC, errno);
        exit(1);
    }

    pthread_t threads[FILL_THREADS]; // Массив потоков (нужно, чтобы потом сделать join)
    int threadArg[FILL_THREADS]; // Буфер, с помощью которого передаются аргументы в новые потоки

    for (int i = 0; i < FILL_THREADS; i++) {
        threadArg[i] = i;
        pthread_create(&threads[i], 0, fillThreadHandler, &threadArg[i]); // Функция fillThreadHandler вызывается с нового потока
    }

    printf("Ожидание завершения потоков...\n");

    for (int i = 0; i < FILL_THREADS; i++) // Ждем, когда все потоки закончат
        pthread_join(threads[i], NULL);

    close(randomFd);
    printf("Область памяти заполнена\n");
}

void writeData() {
    printf("Запись данных из памяти в файлы...\n");

    for (int i = 0; i < FILES_NUMBER; i++) {
        char fileName[] = {'0' + i, '\0'}; // Имя файла

        int actualSize = FILE_SIZE;
        if (i == FILES_NUMBER - 1) // У последнего файла размер может оказаться немного меньше
            actualSize = MMAP_LENGTH - FILE_SIZE*(FILES_NUMBER - 1);

        writeSingleFile(fileName, regionPtr + FILE_SIZE * i, actualSize);
    }
}

void writeSingleFile(char* fileName, void* start, int size) {
    int blocksNumber = size/BLOCK_SIZE; // Считаем количество блоков
    if (BLOCK_SIZE*blocksNumber < size)
        blocksNumber++;
    
    // Откываем файл fileName, O_CREAT позволяет создать файл, если его нет,
    // O_WRONLY открывает файл только для чтения.
    // S_IRWXU, S_IRGRP и S_IROTH - флаги доступа
    // S_IRWXU даёт пользователю (U) все права (RWX)
    // S_IRGRP даёт группе (G, GRP) только право чтения (R)
    // S_IROTH даёт другим (O, OTH) только право чтения (R)
    int fd = open(fileName, O_CREAT | O_WRONLY | O_DIRECT, S_IRWXU | S_IRGRP | S_IROTH);

    if (fd < 0) {
        printf("Не получилось открыть файл '%s' для записи, errno = %d\n", fileName, errno);
        exit(-1);
    }

    fileLock(fd, FILE_LOCK_WRITE);
    for (int i = 0; i < blocksNumber; i++) {
        int effectiveBlockSize = BLOCK_SIZE;
        if (i == blocksNumber - 1) // У последнего блока размер может быть немного меньше
            effectiveBlockSize = size - BLOCK_SIZE*(blocksNumber - 1);

        // Место в памяти, где находится нужный для записи блок
        void* writeStartPtr = start + i*BLOCK_SIZE;

        ssize_t wroteBytes = write(fd, writeStartPtr, effectiveBlockSize);
        printf("Записывается файл '%s': %d/%d блоков\r", fileName, i, blocksNumber);
        fflush(stdout);

        if (wroteBytes == -1) {
            printf("\n");
            printf("Не получилось записать файл '%s', errno = %d\n", fileName, errno);
            exit(-1);
        }
    }

    fileUnlock(fd, FILE_LOCK_WRITE);
    close(fd);
    printf("%c[2K", 27);
    printf("Файл записался!\n");
}

void freeData() {
    munmap(regionPtr,MMAP_LENGTH);
}

void startBackgroundReading() {
    printf("Анализирую содержимое файлов, используя %d потоков...\n", ANALYZING_THREADS);

    pthread_t threads[ANALYZING_THREADS];
    for (int i = 0; i < ANALYZING_THREADS; i++) {
        pthread_create(&threads[i], NULL, fileAnalyzeThreadHandler, NULL);
    }
}

void* fileAnalyzeThreadHandler(void* vargPtr) {
    while(1) {
        for (int i = 0; i < FILES_NUMBER; i++) {
            char fileName[] = {'0' + i, '\0'};

            analyzeFile(fileName);
        }
    }
}

void analyzeFile(char* fileName) {
    int fd = open(fileName, O_CREAT | O_RDONLY, S_IRWXU | S_IRGRP | S_IROTH);

    if (fd < 0) {
        printf("Не получилось открыть файл '%s' для чтения\n", fileName);
        return;
    }

    fileLock(fd, FILE_LOCK_READ);

    // Узнаём размер файла, отправив указатель в конец файла
    off_t size = lseek(fd, 0L, SEEK_END);
    // Возвращаем указатель обратно
    lseek(fd, 0, SEEK_SET);
    
    // Выделаем в памяти место под содержимое файла
    __uint64_t* data = (__uint64_t*) malloc(size);
    // Читаем файл в память целиком
    ssize_t readBytes = read(fd, data, size);

    fileUnlock(fd, FILE_LOCK_READ);
    close(fd);

    // Ищем минимальное 8-байтовое беззнаковое число в файле
    __uint64_t min = data[0];
    for (size_t i = 1; i < readBytes/sizeof(__uint64_t); i += 1) {
        if (data[i] < min)
            min = data[i];
    }

    printf("%c[2K\033[2;49;37mАнализ: Минимальное 8-байтовое беззнаковое число в файле '%s' - %ld\033[0m\n", 27, fileName, min);

    free(data);
}

void fileLock(int fd, int mode) {
    int flockRc; // flock return code

    if (mode == FILE_LOCK_READ) {
        flockRc = flock(fd, LOCK_SH); // Читать множно нескольким потокам/процессам, поэтому LOCK_SH
    } else if (FILE_LOCK_WRITE) {
        flockRc = flock(fd, LOCK_EX); // Писать в файл может только один, поэтому LOCK_EX
    }

    if (flockRc != 0)  {
        printf("Не удалось взять блокировку, errno = %d\n", errno);
        exit(1);
    }

}


void fileUnlock(int fd, int mode) {

    int flockRc = flock(fd, LOCK_UN); // Сброс блокировки

    if (flockRc != 0)  { // Что-то пошло не так
        printf("Не удалось взять блокировку, errno = %d\n", errno);
        exit(1);
    }

}

void* fillThreadHandler(void* vargPtr) { // Это функция, которая выполнится в отдельном потоке
    int chunkSize = MMAP_LENGTH/FILL_THREADS; // Сколько этот поток должен записать (84 МБ / 127 потоков = ~661 Кб на поток)
    int threadIndex = *(int*)vargPtr; // Индекс этого потока (0, 1, 2...)
	void* ptrStart = regionPtr + threadIndex * chunkSize; // Место, откуда надо начать записывать

    // При делении области памяти на количество потоков результат обрезается, поэтому последний поток пишет на несколько байт больше, чтобы восстановить пробел
    if (*(int*)vargPtr == FILL_THREADS - 1) // Этот поток заполняет последнюю часть(надеюсь)
        chunkSize += MMAP_LENGTH - MMAP_LENGTH/FILL_THREADS*FILL_THREADS;

    // Чтение из /dev/urandom в указанную область памяти
    ssize_t result = read(randomFd, ptrStart, chunkSize);

    if (result == -1) {
        printf("Не получилось заполнить область с %p, размер = %d, errno = %d\n", ptrStart, chunkSize, errno);
        exit(-1);
    }
}

// Эта функция вызывается, если пришёл сигнал
void signalHandler(int signum) {
    if (signum == SIGINT) {
        printf("\nЗавершение программы\n");
        exit(signum);
    } else if (signum == SIGSEGV) {
        printf("ПАМАГИТИ! (Segmentation fault)\n");
        exit(signum);
    }
}

void printCat() {
    printf("                    %%                                                         \n");
    printf("                  /&&/&  /&(  *%                                               \n");
    printf("                  &   %%&    &*                                                \n");
    printf("                   #&.%  ,&   ,&,                      #&&# &   &              \n");
    printf("                   &   .  /%&%.%&                      /&  %&   &#  &          \n");
    printf("                  %,     /(                              ##   /&#  &(          \n");
    printf("                  &     *(      .&& (%                  &*  #(  ((  %(         \n");
    printf("                 /#    .&    %&      *%   &&&&/.&      &       &.%&&&          \n");
    printf("                  &     &  .&*         *,        &    .&         &             \n");
    printf("                  &    & *&&.     &  &%(*(%%&    &,  .&        .&              \n");
    printf("                  &   *&%&         &*         &  (#  &        &/               \n");
    printf("                 //   &% &         &,         (#  &,%       %(                 \n");
    printf("                #(       %&%    %&(%         &   &%      *&                    \n");
    printf("                 #/               %%  &&%&&&&.          .&                     \n");
    printf("                 &               %&&&                  &.        /#%(          \n");
    printf("                *%             &&&&&&                 &       &(       &       \n");
    printf("                &.           &&&&&&&                 *&      /%         (*     \n");
    printf("               &              ,&&,                  %/       &          #*     \n");
    printf("               .&                                    &        /%          &    \n");
    printf("               (&                                    &         &          &.   \n");
    printf("               &                                     %/        &,         &    \n");
    printf("       &%   &#&                                      &       #%          &     \n");
    printf("       &       &                                      &&%.  #&           &     \n");
    printf("       &                                              ,       %#        &.     \n");
    printf("       #%                                                      &      ,&       \n");
    printf("       (%&&.                                                  %(   ,&%         \n");
    printf("     #/                                                      &(.               \n");
    printf("      &*&/  .       .(%#((/**,,,,,**/(%&&%%#/,           ,&*  .&               \n");
    printf("     *&.&&&# ,&#.  %%   #%   /    %%  %   #    . &&     % .%  &                \n");
    printf("        ,%      &/  %%   *%   %%  *&   (#  .&( (&  &,  (&.&#                   \n");
    printf("                   .   (        /    *       %                                 \n\n\n\n\n");
}