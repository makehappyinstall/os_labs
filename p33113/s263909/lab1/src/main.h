#ifndef MAIN_H
#define MAIN_H
#define START 0xC5FA086B
#define RES "./res/"
#define FIRST RES"/first"
#define SECOND RES"/second"
#define MFILE "./file"
void write_to_memory(void * pointer);
void *write_thread(void *arg);
void read_from_memory(FILE *file, void * memory_pointer);
void interrupt_signal(int32_t sig);
void* agrigate_state(void*);
void agr_state_thread(void);
int futex_wait(int *uaddr, int val);
int futex_wake(int *uaddr, int val);
struct chunk{
	void * mem_pointer;
	uint64_t size;
	uint64_t start;
};
struct agr_state{
	FILE *fd;
	uint64_t off;
	uint64_t size;
};
enum errors {
	OK,
	FILE_ERR
};
#endif
