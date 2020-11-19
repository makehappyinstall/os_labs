#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdatomic.h>
#include <linux/futex.h>
#include <syscall.h>

const size_t A = 248 * 1024 * 1024;
const uint D = 125;
const size_t E = 195 * 1024 * 1024;
const uint G = 20;
const uint I = 131;

const uint FILL_THREAD_MEMORY = A / D / sizeof(int);
const uint AMOUNT_OF_FILES = 2;
const uint AMOUNT_OF_BLOCKS = E / G;

const char URANDOM_PATH[] = "/dev/urandom";

int urandom_file;
int threads_are_working = 1;

void *allocated_memory;
int *futexes;

typedef struct{
    int index;
    uint size;
} fill_thread_data;

typedef struct{
    int index;
    int file_number;
} read_thread_data;

static void wait_for_futex(int *futex_pointer){
    int set_to_one = 1;
    while (1) {
        if (atomic_compare_exchange_strong(futex_pointer, &set_to_one, 0))
            break;
        syscall(SYS_futex, futex_pointer, FUTEX_WAIT, 0, NULL, NULL, 0);
    }
}

static void wake_futex(int *futex_pointer){
    int set_to_zero = 0;
    if (atomic_compare_exchange_strong(futex_pointer, &set_to_zero, 1)) {
        syscall(SYS_futex, futex_pointer, FUTEX_WAKE, 1, NULL, NULL, 0);
    }
}

void *fill_memory(void *fill_data) {
    fill_thread_data *data = (fill_thread_data*) fill_data;
    void *memory_start_point;
    while (threads_are_working) {
        if (data->index != (D - 1)) {
            memory_start_point = allocated_memory + data->size * data->index;
        } else {
            memory_start_point = allocated_memory + FILL_THREAD_MEMORY * data->index;
        }
        ssize_t urandom_fd = read(urandom_file, memory_start_point, data->size);
        if (urandom_fd < 0){
            printf("Ошибка при чтении файла urandom во время генерации данных.\n");
        }
    }
    return NULL;
}

void start_memory_fill(pthread_t *fill_threads, fill_thread_data *fill_data){
    printf("Запуск потоков для заполнения памяти.\n");
    for (int i = 0; i < D; i++) {
        fill_data[i].index = i;
        fill_data[i].size = FILL_THREAD_MEMORY;
        //printf("Создан поток заполнения памяти с индексом %d.\n", fill_data[i].index);
    }
    fill_data[D - 1].size += A / sizeof(int) % D;
    for (int i = 0; i < D; i++) {
        pthread_create(&fill_threads[i], 0, fill_memory, &fill_data[i]);
    }
}

void write_memory_to_file(int file_number, uint size, void *memory_point){
    char file_name[7] = "file0\0";
    file_name[5] = '0' + file_number;
    uint block_amount = AMOUNT_OF_BLOCKS*G < size ? AMOUNT_OF_BLOCKS + 1 : AMOUNT_OF_BLOCKS;
    wait_for_futex(&futexes[file_number]);
    int file = open(file_name, O_CREAT | O_WRONLY | __O_DIRECT, 00666);
    if (file < 0){
        printf("Ошибка при открытии файла во время записи.\n");
        wake_futex(&futexes[file_number]);
    }
    printf("Начало записи в файл '%s'\n", file_name);
    for (int i = 0; i < block_amount; i++){
        void *current_point = memory_point + i*G;
        uint last_block = block_amount - 1;
        uint size_of_current_block = i == last_block ? (size - G*(last_block)) : G;
        //if (i % 100 == 0)
        //   printf("Записано %d из %d блоков\n", i, block_amount);
        ssize_t current_bytes = write(file, current_point, size_of_current_block);

        if (current_bytes == -1){
            printf("Ошибка при записи в файл.\n");
        }
        if (!threads_are_working) break;
    }
    close(file);
    wake_futex(&futexes[file_number]);
    printf("Запись в файл '%s' завершена успешно.\n", file_name);
}

void* start_writing_to_files(){
    printf("Запуск потока записи в файлы.\n");
    while (threads_are_working) {
        for (int i = 0; i < AMOUNT_OF_FILES; i++) {
            void *memory_point = allocated_memory + E * i;
            write_memory_to_file(i, E, memory_point);
        }
    }
    return NULL;
}

void* read_file(void *read_data){
    read_thread_data *data = (read_thread_data*) read_data;
    while (threads_are_working){
        char file_name[7] = "file0\0";
        file_name[5] = '0' + data->file_number;
        int file = -1;
        while (file == -1){
            wait_for_futex(&futexes[data->file_number]);
            if (!threads_are_working) {
                wake_futex(&futexes[data->file_number]);
                return NULL;
            }
            file = open(file_name, O_RDONLY, 00666);
            if (file == -1) {
                wake_futex(&futexes[data->file_number]);
                //printf("Поток чтения %d | Ошибка при открытии файла %d во время чтения.\n", data->index, data->file_number);
            }
            else {
                //printf("Поток чтения %d | Открыт файл %d для чтения.\n", data->index, data->file_number);
            }
        }
        off_t file_size = lseek(file, 0L, SEEK_END);
        lseek(file, 0, SEEK_SET);
        uint block_amount = file_size / G;
        int *readen_data = (int*) malloc(file_size);
        for (int i = 0; i < block_amount; i++){
            pread(file, readen_data, file_size, G*i);
            //if (i % 100 == 0)
            //   printf("Поток чтения %d | Прочитано %d/%d блоков\n", data->index, i, block_amount);
            if (!threads_are_working) break;
        }
        if (file_size % G > 0){
            pread(file, readen_data, file_size % G, G * block_amount);
        }
        close(file);
        wake_futex(&futexes[data->file_number]);
        if (threads_are_working) {
            long sum = 0;
            for (int i = 0; i < file_size / sizeof(int); i++) {
                sum += readen_data[i];
            }
            //printf("Поток чтения %d | Сумма всех чисел, прочитанных из файла '%s' = %ld\n", data->index, file_name, sum);
        }
        free(readen_data);
    }
    return NULL;
}

void start_file_reading(pthread_t *read_threads, read_thread_data *read_data){
    printf("Запуск потоков для чтения файлов.\n");
    int file_number = 0;
    for (int i = 0; i < I; i++) {

        read_data[i].index = i;
        read_data[i].file_number = file_number++;
        if (file_number >= AMOUNT_OF_FILES){
            file_number = 0;
        }
        //printf("Создан поток чтения с индексом %d, работающий над файлом %d.\n", read_data[i].index, read_data[i].file_number);
    }

    for (int i = 0; i < I; i++) {
        pthread_create(&(read_threads[i]), 0, read_file, &read_data[i]);
    }

}

int main() {
    printf("-> до аллокации памяти.");
    getchar();
    allocated_memory = malloc(A);
    futexes = malloc(sizeof(int) * AMOUNT_OF_FILES);
    urandom_file = open(URANDOM_PATH, O_RDONLY);
    pthread_t *fill_threads = (pthread_t *) malloc(D * sizeof(pthread_t));
    fill_thread_data *fill_threads_data = (fill_thread_data *) malloc(D * sizeof(fill_thread_data));
    pthread_t *read_threads = (pthread_t *) malloc(I * sizeof(pthread_t));
    read_thread_data *read_threads_data = (read_thread_data *) malloc(I * sizeof(read_thread_data));
    pthread_t *writer_thread = (pthread_t *) malloc(sizeof(pthread_t));

    printf("-> после аллокации памяти.");
    getchar();

    for (int i = 0; i < AMOUNT_OF_FILES; ++i) {
        futexes[i] = 1;
    }

    start_memory_fill(fill_threads, fill_threads_data);
    pthread_create(writer_thread, NULL, start_writing_to_files, NULL);
    start_file_reading(read_threads, read_threads_data);

    getchar();

    threads_are_working = 0;
    for (int i = 0; i < D; i++) {
        pthread_join(fill_threads[i], NULL);
    }
    pthread_join(*writer_thread, NULL);
    for (int i = 0; i < I; i++) {
        pthread_join(read_threads[i], NULL);
    }
    printf("Потоки остановлены.\n");
    close(urandom_file);
    free(allocated_memory);
    free(futexes);
    free(fill_threads);
    free(fill_threads_data);
    free(read_threads);
    free(read_threads_data);

    printf("-> после деаллокации памяти.");
    getchar();

    return 0;
}
