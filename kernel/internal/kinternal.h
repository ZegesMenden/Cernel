#ifndef KINTERNAL_H
#define KINTERNAL_H

#include "kernel.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define yield_effective_delay_time_us (threadtime_t)10000

void kernel_init_internal();

// This should be optimally packed? i think?
typedef struct {

    uint8_t* sp_cur;
    uint8_t stack_base[THREAD_STACK_SIZE];
    char* name;

    thread_entrypoint_t entry;
    void* entry_args;

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

corecontext_t* getcorecontext();

extern size_t schedule(threadinfo_t* threads, size_t threadcur, size_t threadcount);

#endif // KINTERNAL_H
