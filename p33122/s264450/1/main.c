#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>

const size_t A = 186 * 1024 * 1024;
void* B = (void*) 0xA6EF4BD5;
void* page_aligned_B = (void*) 0xA6EF4000;
const int D = 32;
const int E = 112 * 1024 * 1024;
const size_t G = 37;
const int I = 27;
const int file_number = 20;
_Atomic unsigned long long processed_files_count = 0;
sem_t files_semaphore;


struct memory_filler_data {
	size_t part_length;
	void* address;
	FILE* random_file;
};

void* memory_filler_thread(void* in_thread_data) {
	struct memory_filler_data thread_data = *(struct memory_filler_data*) in_thread_data;
	fread(thread_data.address, thread_data.part_length, 1, thread_data.random_file);
	free(in_thread_data);
	pthread_exit(NULL);
}

_Noreturn void* file_aggregator_thread() {
	while (1) {
		sem_wait(&files_semaphore);
		printf("Aggregator thread awakened!\n");
		unsigned long long current_file = processed_files_count++;
		char filename[20];
		sprintf(filename, "%llu", current_file);

		long long sum = 0;
		int num;

		FILE* output_file = fopen(filename, "r");

		while (fread(&num, sizeof(int), 1, output_file) == 1) {
			sum += num;
		}

		fclose(output_file);
		printf("My current file is: %llu It's sum is: %lld\n", current_file, sum);

		remove(filename);
	}
	pthread_exit(NULL);
}

void fill_memory(void* memory_address, size_t memory_size) {

	//before allocation

	mmap(memory_address, memory_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

	//after allocation

	FILE* random_file = fopen("/dev/urandom", "r");

	pthread_t filler_threads[D];
	size_t part_length = memory_size / D;
	for (int i = 0; i < D; ++i) {
		struct memory_filler_data* thread_data = malloc(sizeof(struct memory_filler_data));

		thread_data->part_length = part_length;
		thread_data->address = (char*) memory_address + part_length * i;
		thread_data->random_file = random_file;

		pthread_create(&filler_threads[i], NULL, memory_filler_thread, (void*) thread_data);
	}

	for (int i = 0; i < D; ++i) {
		pthread_join(filler_threads[i], NULL);
	}

	fclose(random_file);

	//after memory fill
}

void fill_file_from_memory(void* memory_address, size_t data_length, char* filename) {
	FILE* output_file = fopen(filename, "w");
	fwrite(memory_address, G, data_length / G + 1, output_file);
	fclose(output_file);
}

void* fill_memory_write_file() {
	sem_init(&files_semaphore, 0, 0);
	unsigned long long created_files_count = 0;
	while (1) {
		//    for (int i = 0; i < file_number; ++i) {
		fill_memory(B, A);
		char* filename = malloc(20 * sizeof(char));
		sprintf(filename, "%llu", created_files_count);
		printf("Current file: %s\n", filename);
		fill_file_from_memory(B, E, filename);

		created_files_count++;
		sem_post(&files_semaphore);

		munmap(page_aligned_B, A);
		free(filename);

		// after deallocating
	}
	sem_destroy(&files_semaphore);
	pthread_exit(NULL);
}

void* aggregate_files() {
	sleep(25);
	pthread_t aggregation_threads[I];
	for (int i = 0; i < D; ++i) {
		pthread_create(&aggregation_threads[i], NULL, file_aggregator_thread, NULL);
	}
	pthread_exit(NULL);
}

int main() {
	pthread_t* memory = malloc(sizeof(pthread_t));
	pthread_t* files = malloc(sizeof(pthread_t));

	pthread_create(memory, NULL, fill_memory_write_file, NULL);
	pthread_create(files, NULL, aggregate_files, NULL);

	pthread_exit(NULL);
}
