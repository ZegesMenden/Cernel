#ifndef PICO_KERNEL_H
#define PICO_KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef MAX_THREADS
#define MAX_THREADS 16
#endif

#ifndef THREAD_STACK_SIZE
#define THREAD_STACK_SIZE 4096
#endif

typedef uint8_t threadpriority_t;
#define thread_idle_priority (threadpriority_t)0

typedef uint64_t threadtime_t;
typedef int64_t threadtimediff_t;
#define thread_nodelay (threadtime_t)0
#define thread_yielddelay (threadtime_t)UINT64_MAX
#define thread_maxtime (threadtime_t)UINT64_MAX

typedef void(*thread_entrypoint_t)(void*);
void thread_create(thread_entrypoint_t entry, void* args, threadpriority_t priority, const char* name);

void thread_begin();
void thread_yield(threadtime_t delay);

threadtime_t thread_gettime();
uint64_t thread_timetous(threadtime_t t);
uint64_t thread_timetoms(threadtime_t t);
uint64_t thread_timetos(threadtime_t t);

threadtime_t thread_ustotime(uint64_t t_us);
threadtime_t thread_mstotime(uint64_t t_ms);
threadtime_t thread_stotime(uint64_t t_s);


#endif // PICO_KERNEL_H
