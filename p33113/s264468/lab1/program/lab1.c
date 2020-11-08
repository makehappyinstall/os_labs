#include <fcntl.h>
#include <linux/futex.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MEMORY_SIZE (178 * 1024 * 1024)
#define DESIRED_ADDRESS 0xDF22D146
#define URANDOM_THREAD_NUMBER 32
#define URANDOM_THREAD_MEMORY_SIZE ((MEMORY_SIZE) / (URANDOM_THREAD_NUMBER))
#define FILE_SIZE (83 * 1024 * 1024)
#define FILE_NUMBER (((MEMORY_SIZE) + (FILE_SIZE)-1) / (FILE_SIZE))
#define FILE_START_SHIFT (((MEMORY_SIZE) - (FILE_SIZE)) / ((FILE_NUMBER)-1))
#define WRITE_BLOCK_SIZE 61
#define READ_THREAD_NUMBER 37
#define READ_THREADS_PER_FILE ((READ_THREAD_NUMBER) / (FILE_NUMBER))

#define futex(uaddr, futex_op, val, timeout, uaddr2, val3)                     \
  syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3)

void futex_wait(int *futexp) {
  for (;;) {
    int one = 1;
    if (atomic_compare_exchange_strong(futexp, &one, 0))
      break;
    futex(futexp, FUTEX_WAIT, 0, NULL, NULL, 0);
  }
}

void futex_wake(int *futexp) {
  int zero = 0;
  if (atomic_compare_exchange_strong(futexp, &zero, 1))
    futex(futexp, FUTEX_WAKE, 1, NULL, NULL, 0);
}

struct ThreadInfo {
  pthread_t id;
  size_t number;
  long long result;
};

char FILENAMES[FILE_NUMBER][256];
int memory_futexes[URANDOM_THREAD_NUMBER];
int file_futexes[READ_THREAD_NUMBER];
char *allocated_memory;
struct ThreadInfo generate_thread_info[URANDOM_THREAD_NUMBER];
struct ThreadInfo write_thread_info[FILE_NUMBER];
struct ThreadInfo read_thread_info[READ_THREAD_NUMBER];

void *fill_memory(void *param) {
  struct ThreadInfo *info = param;
  int fd = open("/dev/urandom", O_RDONLY);
  futex_wait(&memory_futexes[info->number]);
  read(fd, allocated_memory + URANDOM_THREAD_MEMORY_SIZE * info->number,
       URANDOM_THREAD_MEMORY_SIZE);
  futex_wake(&memory_futexes[info->number]);
  close(fd);
  return NULL;
}

void *write_file(void *param) {
  struct ThreadInfo *info = param;
  char *mem_start = allocated_memory + FILE_START_SHIFT * info->number;
  size_t pos;
  for (pos = FILE_START_SHIFT * info->number / URANDOM_THREAD_MEMORY_SIZE;
       URANDOM_THREAD_MEMORY_SIZE * pos <
       FILE_SIZE + FILE_START_SHIFT * info->number;
       ++pos)
    futex_wait(&memory_futexes[pos]);
  for (size_t i = info->number; i < READ_THREAD_NUMBER; i += FILE_NUMBER)
    futex_wait(&file_futexes[i]);

  int fd = open(FILENAMES[info->number], O_WRONLY | O_CREAT | O_TRUNC,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  for (pos = 0; pos + WRITE_BLOCK_SIZE <= FILE_SIZE; pos += WRITE_BLOCK_SIZE)
    write(fd, mem_start + pos, WRITE_BLOCK_SIZE);
  if (pos != FILE_SIZE)
    write(fd, mem_start + pos, FILE_SIZE - pos);
  close(fd);

  for (pos = FILE_START_SHIFT * info->number / URANDOM_THREAD_MEMORY_SIZE;
       URANDOM_THREAD_MEMORY_SIZE * pos <
       FILE_SIZE + FILE_START_SHIFT * info->number;
       ++pos)
    futex_wake(&memory_futexes[pos]);
  for (size_t i = info->number; i < READ_THREAD_NUMBER; i += FILE_NUMBER)
    futex_wake(&file_futexes[i]);
  return NULL;
}

void *read_file(void *param) {
  struct ThreadInfo *info = param;
  size_t my_file = info->number % FILE_NUMBER;
  futex_wait(&file_futexes[my_file]);
  size_t readers_total = (READ_THREAD_NUMBER - my_file) / FILE_NUMBER;
  size_t my_pos = info->number / FILE_NUMBER * FILE_SIZE / readers_total;
  size_t my_size = my_pos + FILE_SIZE / readers_total > FILE_SIZE
                       ? FILE_SIZE / readers_total
                       : FILE_SIZE - my_pos;
  int fd = open(FILENAMES[my_file], O_RDONLY);
  void *buffer = malloc(my_size);
  read(fd, buffer, my_size);

  int *iptr = buffer;
  long long result = 0;
  for (size_t i = 0; 4 * (i + 1) <= my_size; ++i)
    result += (long long)iptr[i];
  info->result = result;

  free(buffer);
  close(fd);
  futex_wake(&file_futexes[my_file]);
  return NULL;
}

int main() {
  for (int i = 0; i < FILE_NUMBER; ++i)
    sprintf(FILENAMES[i], "file%d.dat", i);
  for (size_t i = 0; i < URANDOM_THREAD_NUMBER; ++i)
    memory_futexes[i] = 1;
  for (size_t i = 0; i < READ_THREAD_NUMBER; ++i)
    file_futexes[i] = 1;

  allocated_memory = malloc(MEMORY_SIZE);

  for (;;) {
    for (size_t i = 0; i < URANDOM_THREAD_NUMBER; ++i) {
      generate_thread_info[i].number = i;
      pthread_create(&generate_thread_info[i].id, NULL, fill_memory,
                     &generate_thread_info[i]);
    }

    for (size_t i = 0; i < FILE_NUMBER; ++i) {
      write_thread_info[i].number = i;
      pthread_create(&write_thread_info[i].id, NULL, write_file,
                     &write_thread_info[i]);
    }

    for (size_t i = 0; i < READ_THREAD_NUMBER; ++i) {
      read_thread_info[i].number = i;
      pthread_create(&read_thread_info[i].id, NULL, read_file,
                     &read_thread_info[i]);
    }

    for (size_t i = 0; i < URANDOM_THREAD_NUMBER; ++i)
      pthread_join(generate_thread_info[i].id, NULL);

    for (size_t i = 0; i < FILE_NUMBER; ++i)
      pthread_join(write_thread_info[i].id, NULL);

    long long sum = 0;
    for (size_t i = 0; i < READ_THREAD_NUMBER; ++i) {
      pthread_join(read_thread_info[i].id, NULL);
      sum += read_thread_info[i].result;
    }
    printf("The sum is %20lld\n", sum);
  }
  return 0;
}
