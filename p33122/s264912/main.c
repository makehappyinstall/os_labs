#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include "./main.h"

static sem_t fileSync;

int main(int args, char *argv[]){
	// до аллокации
	puts("Before allocate");
	getchar();
	
	void* memory = (void *) 0x5845DAC9;
	// mmap(addr, lenght, prot, flags, fd, offset)
	// addr = memory
	// length = 46 mb = 46*1024*1024 b
	// разместили память рядом с выбранной
	// prot = разрешение даем на чтение и запись
	// flags = не используем файл и совместо с другими процессами
	// fd = раз файла нет то -1
	// offset = смещение 0
	memory = mmap(memory, 46*1024*1024,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0);
	assert(memory != MAP_FAILED);
	
	//после аллокации
	puts("After allocate");
	getchar();
	signal(SIGINT, close_signal);
	while(1){
		long total_size = 46*1024*1024;
		long remaining_size = total_size;
		long size_for_one_thread = 3014656; //total_size / 16
		pthread_t thread_id;
		for (char i = 0; i < 16; i++){
			struct portion *block = malloc(sizeof(struct portion));//будет указатель на адрес первого байта структуры
			if((total_size -= size_for_one_thread) < size_for_one_thread){
				block->memory_pointer = memory;
				block->size = size_for_one_thread;
				block->offset = total_size - remaining_size;
				
				//pthread_create(threadid, attr, start_routing, arg);
				pthread_create(&thread_id, NULL, write_thread, block);
			}
		}
		//pthread_join(thread, retval)
		// тут нужен чтоб дождаться завершения потока
		pthread_join(thread_id, NULL);
		
		// OPEN
		// 46 / 25 = 2 files
		
		int file1 = open("./file1", O_WRONLY | O_CREAT | __O_DIRECT, 00666);
		if (file1 == -1){
			perror("Error with file1");
			exit(FILE_ERR);
		}
		read_from_memory(file1, memory);
		close(file1);
		
		int file2 = open("./file2", O_WRONLY | O_CREAT | __O_DIRECT, 006660);
		if (file2 == -1){
			perror("Error with file2");
			exit(FILE_ERR);
		}
		read_from_memory(file2, memory+25*1024*1024-0x5845DAC9);
		close(file2);
		
		init_sem();

		long long int sum = 0;
		void * results = malloc(101*sizeof(unsigned long long int));
		long *all_result;
		pthread_t threads_id[101];
		long size = 25*1024*1024/50;
		long remainder = 25*1024*1024%50;
		long offset = 0;
		FILE *f1 = fopen("./file1", "rb");
		FILE *f2 = fopen("./file2", "rb");
		assert((f1 != NULL) && (f2 != NULL));
		for (char i = 0; i < 99; i+=2){
			struct state * state1 = malloc(sizeof(struct state));
			struct state * state2 = malloc(sizeof(struct state));
			state1->fd = f1;
			state2->fd = f2;
			state1->size = size;
			state2->size = size;
			state1->offset = offset;
			state2->offset = offset;
			pthread_create(&(threads_id[i]), NULL, read_and_sum, state1);
			pthread_create(&(threads_id[i+1]), NULL, read_and_sum, state2);
			offset += size;
			
		}
		struct state * state1 = malloc(sizeof(struct state));
		state1->fd = f1;
		state1->size = remainder;
		state1->offset = offset;
		pthread_create(&(threads_id[100]), NULL, read_and_sum, state1);

		
		for(char i = 0; i < 101; i++){
			pthread_join(threads_id[i], results+(i*sizeof(long)));
		}
		all_result = results;
		for(char i = 0; i < 101; i++){
			sum += all_result[i];
		}
		printf("%lld\n",sum);
		
		//после аллокации
		puts("After writting");
		getchar();
	}
	
	return OK;
}

void init_sem(){
	sem_init(&fileSync, 0, 1);
}
void *write_thread(void *arg){
	struct portion *block = (struct portion*) arg;
	FILE *random = fopen("/dev/urandom", "r"); 
	fread(block->memory_pointer + block->offset, 1, block->size, random);
	return 0;
}

void read_from_memory(int file, void * memory_pointer){
	for (int counter = 0; counter < 25*1024*1024; counter += 38) {
		write(file, memory_pointer+counter, sizeof(char)*38);
	}
}


void *read_and_sum(void* arg){
	struct state * state = (struct state*) arg;
	long sum = 0;
	long point = 0;
	sem_wait(&fileSync);
	fseek(state->fd, state->offset, SEEK_SET); //переставляет указатель в файле
	for (long counter = 0; counter != state->size; counter++){
		point = fgetc(state->fd);//получает один char из файла
		sum += point;
	}
	sem_post(&fileSync);
	return (void*)sum;
}

void close_signal(int32_t sig){
	puts("affter deallocat");
	getchar();
	exit(0);
};
