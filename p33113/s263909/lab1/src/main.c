#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <signal.h>
#include <linux/futex.h>
#include <syscall.h>
#include "../lib/flags.h"
#include "./main.h"

const unsigned char preallocate = 	0x1;	//0000 0001
const unsigned char afteralloc = 	0x2;	//0000 0010
const unsigned char afterfill = 	0x4;	//0000 0100
const unsigned char dealloc = 		0x8;	//0000 1000
int flags = 0; //standart flag
int futex = 0;
void *addr;

int main(int args, char * argv[]){
	//check flags
	//example: ./main -alloc=x, x={0,1,2,4,8}
	initFlag(args, argv);
	flag_int(&flags,"alloc");
	
	//I well use it to break cycle (^C)
	signal(SIGINT, interrupt_signal);

	//check do we need dump before allocate
	if (flags & preallocate){
		puts("it's time to check memory(pre allocation)\npress enter to continue");
		getchar();
	}
	
	void* memory_pointer =  (void *) START;
	
	/**There we allocate memory close to chosen memory
	 * 242 Mb = 242*1024*1024
	 * with permission to read and write
	 * shared with other process and don't use file (anonimus)
	 * because of anonimus file descriptor is -1
	 * offset is 0 by the rules*/
	int file = open("./file", O_RDWR);
	ftruncate(file, 242*1024*1024);
	memory_pointer = mmap(memory_pointer,242*1024*1024,PROT_READ|PROT_WRITE,MAP_PRIVATE,file,0);
	addr = memory_pointer;
	//assert will output error if there is something wrong
	assert(memory_pointer != MAP_FAILED);

	if (flags & afteralloc){
		puts("it's time to check memorry(afret allocation)\npress enter to continue");
		getchar();
	}

	//endless cycle starts here
	while(1){
		puts("<--cycle start-->");
		write_to_memory(memory_pointer);

		FILE *first = fopen("./res/first", "wb");
		if (first == NULL){
			perror("first file error");
			exit(FILE_ERR);
		}
		read_from_memory(first, memory_pointer);
		fclose(first);

		FILE *second = fopen("./res/second", "wb");
		if (second == NULL) {
			perror("second file error");
			exit(FILE_ERR);
		}
		read_from_memory(second, (memory_pointer+162*1024*1024-0x5200059));
		fclose(second);
		agr_state_thread();

		if (flags & afterfill) {
			puts("it's time to check memorry(after writting)\npress enter to continue");
			getchar();
		}
		puts("<--cycle end-->");
	}

	return OK;
}
/**Thit function accepts memory pointer
 * start threads and make structures for thread's function
 * 35 threads for 242mb which is
 * 7 250 154 bytes for 1 thread and remainder 2 bytes*/
void write_to_memory(void * memory_pointer){
	uint64_t total = 242*1024*1024;
	uint64_t blockSize = 7250154;
	pthread_t thread_id;
	for (uint8_t i = 0; i < 35 ; i++) {
		struct chunk *piece = malloc(sizeof(struct chunk));
		if ((total -= blockSize) < blockSize){
			blockSize += total;
			total = 0;
		}
		piece->mem_pointer = memory_pointer;
		piece->size = blockSize;
		piece->start = total;

		pthread_create(&thread_id, NULL, write_thread, piece);
	}
}

/**This function will write directly into the memory
 * fopen seems better idea than cycles
 * we read data from urandom and place it in memory
 * great isn't it?*/
void *write_thread(void *arg){
	struct chunk *piece = (struct chunk*) arg;
	FILE *urand = fopen("/dev/urandom", "r");
	fread((piece->mem_pointer + piece->start), 1, piece->size, urand);
	return 0;
}

/**This function reads 162 Mb from memory to file*/
void read_from_memory(FILE *file, void * memory_pointer){
	for (uint32_t counter = 0; counter < 162*1024*1024; counter += 129) {
		fwrite(memory_pointer+counter, sizeof(uint8_t), 129, file);
	}
}

/**This function create threads
 * and threads arguments (struct agr_state)
 * which contains meta data
 * then compute sum of threads returns*/
void agr_state_thread(void){
	uint64_t sum = 0;
	void * results = malloc(46*sizeof(uint64_t));
	uint64_t *resultss;
	pthread_t tid[46];
	uint64_t size = 162*1024*1024/23;
	uint64_t remainder = 162*1024*1024%23;
	uint64_t offset = 0;
	FILE *f1 = fopen("./res/first", "rb");
	FILE *f2 = fopen("./res/second", "rb");
	assert((f1!=NULL)&&(f2!=NULL));
	for (uint8_t i = 0; i<45; i+=2) {
		if (i == 44)
			size = remainder;
		struct agr_state *state = malloc(sizeof(struct agr_state));
		struct agr_state *state2 = malloc(sizeof(struct agr_state));
		state->fd = f1;
		state2->fd = f2;
		state->size = size;
		state2->size = size;
		state->off = offset;
		state2->off = offset;
		pthread_create(&(tid[i]),NULL,agrigate_state, state);
		pthread_create(&(tid[i+1]),NULL,agrigate_state, state2);
		offset += size;
	}
	futex_wake(&futex,1);
	for (uint8_t i = 0; i<46; i++) {
		pthread_join(tid[i], results+(i*sizeof(uint64_t)));
	}
	resultss = results;
	for (uint8_t i = 0; i<46; i++) {
		sum += resultss[i];
	}
	printf("files data sum -> %"PRId64"\n",sum);
}

/**This function read chunk of data from file
 * and compute sum of the data*/
void *agrigate_state(void* arg){
	struct agr_state * state = (struct agr_state*) arg;
	uint64_t sum = 0;
	uint8_t point = 0;
	futex_wait(&futex, 0);
	fseek(state->fd, state->off, SEEK_SET);
	for (uint64_t counter = 0; counter != state->size; counter++) {
		point = fgetc(state->fd);
		sum += point;
	}
	futex_wake(&futex, 1);
	return (void*)sum;
}

/**This function was created to take interrupt signal
 * and break endless cycle
 * It allows us to read system data*/
void interrupt_signal(int32_t sig){
	munmap(addr, 242*1024*1024);
	if (flags & dealloc) {
		puts("it's time to check memory after dealloc\npress enter to continue");
		getchar();
	}
	exit(0);
}

//system calls for futex
int futex_wait(int *uaddr, int val){
	return syscall(SYS_futex, uaddr, FUTEX_WAIT, val, NULL, NULL, 0);
}

int futex_wake(int *uaddr, int val){
	return syscall(SYS_futex, uaddr, FUTEX_WAKE, val, NULL, NULL, 0);
}
