#ifndef KINTERNAL_H
#define KINTERNAL_H

#include "kthreads.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define TID_UNUSED_MEMORY   (tid_t)0
#define TID_STREAM_MEMORY   (tid_t)1
#define TID_FIRST_VALID     (tid_t)8

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

    uint32_t critical_irqstate;
    uint32_t critical_depth;

    threadpriority_t priority;

    bool active;

} threadinfo_t;

typedef struct {

    threadinfo_t thread_list[MAX_THREADS];
    size_t threadcount;
    size_t threadcur;

    uint32_t irqstate;
    unsigned int critical_depth;

} corecontext_t;

void kernel_init_internal();

corecontext_t* getcorecontext();
bool context_is_initialized();

extern size_t schedule(threadinfo_t* threads, size_t threadcur, size_t threadcount);

extern void kernel_memory_thread_exit(tid_t tid);

tid_t get_calling_tid();

/// @brief Consume count blocks from system memory for kernel-owned storage.
/// @param count Number of blocks to consume for this resource, bytes consumed is equal to: count*KMEMORY_BLOCK_SIZE
/// @param memory_type Reserved kernel memory type flag. Must be greater than TID_UNUSED_MEMORY and below TID_FIRST_VALID.
/// @param data Pointer containing a pointer to the first byte of allocated memory
/// @return number of blocks allocated, or -1 if memory is not available or the type flag is invalid
extern int mem_block_consume_kernel(uint16_t count, tid_t memory_type, void** data);

/// @brief Return kernel-owned blocks to memory.
/// @param mem Pointer to the first byte of data in the block. If other blocks were allocated in the same call as the block being freed, they will be freed as well.
/// @return number of blocks freed, or -1 if errors were encountered
int mem_block_return_kernel(void* mem);

#endif // KINTERNAL_H
