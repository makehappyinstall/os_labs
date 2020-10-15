#ifndef __TPOOL_H__
#define __TPOOL_H__

#include <stddef.h>
#include <stdbool.h>

struct tpool;
typedef struct tpool tpool_t;

struct tpool_work;
typedef struct tpool_work tpool_work_t;

typedef void (*thread_func_t)(void *arg);

tpool_t *tpool_create(size_t num);
void tpool_destroy(tpool_t *tm);

tpool_work_t *tpool_add_work(tpool_t *tm, thread_func_t func, void *arg, bool await);
void tpool_wait_work(tpool_work_t *work);
void tpool_wait(tpool_t *tm);

#endif /* __TPOOL_H__ */
