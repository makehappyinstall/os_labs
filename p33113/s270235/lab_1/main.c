#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>

#define MEMORY_SIZE (196 * 1024 * 1024)
#define START_ADDRESS 0xA240B573
#define RANDOM_THREAD_NUM 70
#define RANDOM_THREAD_MEMORY ((MEMORY_SIZE) / (RANDOM_THREAD_NUM))
#define FILE_SIZE (130 * 1024 * 1024)
#define BLOCK_SIZE 16
#define READ_THREAD_NUM 110

struct portion {
  void *mem_pointer;
  uint64_t size;
  uint64_t start;
};

struct thread_args {
  FILE *file_d;
  uint64_t size;
  uint64_t offset;
};

void thread_init_for_write(void *memory_pointer);
void *write_to_memory(void *struct_address);
void read_from_memory_to_file(FILE *file, void *memory_pointer);
uint64_t thread_init_for_read(void);
void *func_sum(void *th_args);
void i_signal(int32_t signal);

sem_t mutex;  // control access to reader count
sem_t resourse; // control access to resourse (in our case write to file)
uint8_t readers = 0; // number of threads reading the resourse
void *address_del;

int main(int argc, char **argv) {

	if(argc > 1) {
  signal(SIGINT, i_signal);
  }

  if (argc > 1) {
     puts("Pre allocation \nPress to continue");
    getchar();
  }

  void *memory_pointer = (void *)START_ADDRESS;

  int file = open("./res/file", O_RDWR);
  ftruncate(file, MEMORY_SIZE);

  memory_pointer = mmap(memory_pointer, MEMORY_SIZE, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE, file, 0);
  address_del = memory_pointer;

  sem_init(&mutex, 0, 1);
  sem_init(&resourse, 0, 1);

  if (memory_pointer == MAP_FAILED) {
    puts("something wrong with memory allocation");
    exit(1);
  }

  if (argc > 1) {
    puts("After allocation \nPress to continue");
    getchar();
  }

  while (1) {
    puts("------ start ------\n");
    thread_init_for_write(memory_pointer);

    if (argc > 1) {
      puts("After filling the section with data \nPress to continue");
      getchar();
    }

    FILE *f_1 = fopen("./res/file_1", "wb");
    if (f_1 == NULL) {
      puts("First file error\n");
      exit(1);
    }

    read_from_memory_to_file(f_1, memory_pointer);
    fclose(f_1);

    FILE *f_2 = fopen("./res/file_2", "wb");
    if (f_2 == NULL) {
      puts("Second file error\n");
      exit(1);
    }
    read_from_memory_to_file(
        f_2, (void *)((uint64_t)memory_pointer + (66 * 1024 * 1024)));
    fclose(f_2);

    uint64_t sum = thread_init_for_read();
    printf(" summ: %" PRIo64 "  \n", sum);

    puts("\n------ end ------\n");
  }
}

void thread_init_for_write(void *memory_pointer) {
  pthread_t threads[RANDOM_THREAD_NUM];
  for (uint8_t i = 0; i < RANDOM_THREAD_NUM; i++) {
    struct portion *part = malloc(sizeof(struct portion));
    part->mem_pointer = memory_pointer;
    part->size = RANDOM_THREAD_MEMORY;
    part->start = i;

    pthread_create(&threads[i], NULL, write_to_memory, part);
  }

  for (uint8_t i = 0; i < RANDOM_THREAD_NUM; i++) {
    pthread_join(threads[i], NULL);
  }
}

void *write_to_memory(void *struct_address) {
  FILE *f_urand = fopen("/dev/urandom", "r");
  struct portion *part = (struct portion *)struct_address;
  fread((void *)((uint64_t)part->mem_pointer + (part->start * part->size)), 1,
        part->size, f_urand);
  return NULL;
}

void read_from_memory_to_file(FILE *file, void *memory_pointer) {
  sem_wait(&resourse);
  for (uint32_t i = 0; i < (FILE_SIZE / BLOCK_SIZE); i++) {
    fwrite((void *)((uint64_t)memory_pointer + BLOCK_SIZE * i), sizeof(char),
           BLOCK_SIZE, file);
  }
  sem_post(&resourse);
}

uint64_t thread_init_for_read(void) {
  pthread_t threads_id[READ_THREAD_NUM];
  uint64_t size = 2 * FILE_SIZE / READ_THREAD_NUM;
  uint64_t offset = 0;
  FILE *file_1 = fopen("./res/file_1", "rb");
  FILE *file_2 = fopen("./res/file_2", "rb");
  if (file_1 == NULL || file_2 == NULL) {
    puts("can not open the files");
    exit(1);
  }
  for (uint8_t i = 0; i < READ_THREAD_NUM - 1; i += 2) {
    struct thread_args *args_1 =
        (struct thread_args *)malloc(sizeof(struct thread_args));
    struct thread_args *args_2 =
        (struct thread_args *)malloc(sizeof(struct thread_args));

    args_1->file_d = file_1;
    args_2->file_d = file_2;

    args_1->size = size;
    args_2->size = size;

    args_1->offset = offset;
    args_2->offset = offset;

    offset += size;

    pthread_create(&threads_id[i], NULL, func_sum, args_1);
    pthread_create(&threads_id[i + 1], NULL, func_sum, args_2);
  }

  void *res = malloc(sizeof(uint64_t) * READ_THREAD_NUM);
  uint64_t sum = 0;

  for (uint8_t i = 0; i < READ_THREAD_NUM; i++) {
    pthread_join(threads_id[i],
                 (void *)((uint64_t)res + (i * sizeof(uint64_t))));
  }

  uint64_t *result = (uint64_t *)res;

  for (uint8_t i = 0; i < READ_THREAD_NUM; i++) {
    sum += result[i];
  }
  return sum;
}

void *func_sum(void *th_args) {

  sem_wait(&mutex);
  readers++;
  if(readers == 1) {
  	sem_wait (&resourse);
  }
  sem_post(&mutex);

  struct thread_args *t_args = (struct thread_args *)th_args;
  uint64_t sum = 0;
  char *var = (char *)malloc(sizeof(char) * t_args->size);
  fseek(t_args->file_d, t_args->offset, SEEK_SET);
  fread(var, sizeof(char), t_args->size, t_args->file_d);
  for (uint64_t i = 0; i < t_args->size; i++) {
    sum += var[i];
  }
  free(var);
  sem_wait(&mutex);
  readers--;
  if (readers == 0) {
    sem_post(&resourse);
  }
  sem_post(&mutex);
  return (void *)sum;
}

void i_signal(int32_t signal) {
  munmap(address_del, MEMORY_SIZE);
  puts("After deallocation \nPress to continue");
  getchar();
  exit(0);
}
