#ifndef KINTERNAL_H
#define KINTERNAL_H

#include "kthreads.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define yield_effective_delay_time_us (threadtime_t)10000

typedef struct {

    uint8_t* sp_cur;
    uint8_t stack_base[THREAD_STACK_SIZE];
    const char* name;

    thread_entrypoint_t entry;
    void* entry_args;

    tid_t tid;

    threadtime_t lastrun;
    threadtime_t nextrun;

    size_t sp_size;

    threadpriority_t priority;

    bool active;

} threadinfo_t;

typedef struct {

    threadinfo_t thread_list[MAX_THREADS];
    size_t threadcount;
    size_t threadcur;

} corecontext_t;

void kernel_init_internal();

corecontext_t* getcorecontext();
bool context_is_initialized();

extern size_t schedule(threadinfo_t* threads, size_t threadcur, size_t threadcount);

extern void kernel_memory_thread_exit(tid_t tid);

tid_t get_calling_tid();

#endif // KINTERNAL_H
