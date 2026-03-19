/* concat.py output: concatenated sources */
/* DO NOT EDIT: re-run concat.py instead. */

/* ===== BEGIN FILE: kernel\include\kernel.h ===== */
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
void thread_create(thread_entrypoint_t entry, void* args, threadpriority_t priority, char* name);

void thread_begin();
void thread_yield(threadtime_t delay);

threadtime_t thread_gettime();
uint64_t thread_timetous(threadtime_t t);
uint64_t thread_timetoms(threadtime_t t);
uint64_t thread_timetos(threadtime_t t);

#endif // PICO_KERNEL_H

/* ===== END FILE: kernel\include\kernel.h ===== */

/* ===== BEGIN FILE: kernel\include\klogging.h ===== */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "kernel.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

const static uint8_t LOG_LEVEL_ERROR = 1;
const static uint8_t LOG_LEVEL_WARN = 2;
const static uint8_t LOG_LEVEL_INFO = 4;
const static uint8_t LOG_LEVEL_VERBOSE = 8;

const static char* ANSI_DISABLE = "\033[0m";
const static char* ANSI_COLOR_RED = "\033[31m";
const static char* ANSI_COLOR_GREEN = "\033[32m";
const static char* ANSI_COLOR_YELLOW = "\033[33m";

const static int __log_level_global = LOG_LEVEL;

void __log_printf(int level, const char* tag, const char* format, ...);

#define LOGV(tag, ...) __log_printf(LOG_LEVEL_VERBOSE, tag, __VA_ARGS__)
#define LOGI(tag, ...) __log_printf(LOG_LEVEL_INFO, tag, __VA_ARGS__)
#define LOGW(tag, ...) __log_printf(LOG_LEVEL_WARN, tag, __VA_ARGS__)
#define LOGE(tag, ...) __log_printf(LOG_LEVEL_ERROR, tag, __VA_ARGS__)

void _test_fn(const char *fname, int result, const char *_tag);

#define test_fn(fun) _test_fn(#fun, fun, TAG)

#ifdef __cplusplus
}
#endif

/* ===== END FILE: kernel\include\klogging.h ===== */

/* ===== BEGIN FILE: kernel\internal\kinternal.h ===== */
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

/* ===== END FILE: kernel\internal\kinternal.h ===== */

/* ===== BEGIN FILE: kernel\context.s ===== */
.section .text
.global thread_context_switch_coop
.type thread_context_switch_coop, @function

.global thread_context_init
.type thread_context_init, @function

.global thread_context_set
.type thread_context_set, @function

.equ thread_context_size, 64

thread_context_switch_coop:

    lw t0, 0(a1) # load current stack pointer
    
    # store current thread info in stack
    addi sp, sp, -thread_context_size
    sw   ra,     0(sp)
    sw   s0,     4(sp)
    sw   s1,     8(sp)
    sw   s2,    12(sp)
    sw   s3,    16(sp)
    sw   s4,    20(sp)
    sw   s5,    24(sp)
    sw   s6,    28(sp)
    sw   s7,    32(sp)
    sw   s8,    36(sp)
    sw   s9,    40(sp)
    sw   s10,   44(sp)
    sw   s11,   48(sp)
    
    # *cur_sp = current saved frame
    sw   sp, 0(a0)

    # sp = next saved frame
    mv   sp, t0

    # load thread info into registers
    lw   ra,     0(sp)
    lw   s0,     4(sp)
    lw   s1,     8(sp)
    lw   s2,    12(sp)
    lw   s3,    16(sp)
    lw   s4,    20(sp)
    lw   s5,    24(sp)
    lw   s6,    28(sp)
    lw   s7,    32(sp)
    lw   s8,    36(sp)
    lw   s9,    40(sp)
    lw   s10,   44(sp)
    lw   s11,   48(sp)
    addi sp, sp, thread_context_size

    ret

thread_context_set:
    
    # sp = next saved frame
    mv   sp, a0

    # load thread info into registers
    lw   ra,     0(sp)
    lw   s0,     4(sp)
    lw   s1,     8(sp)
    lw   s2,    12(sp)
    lw   s3,    16(sp)
    lw   s4,    20(sp)
    lw   s5,    24(sp)
    lw   s6,    28(sp)
    lw   s7,    32(sp)
    lw   s8,    36(sp)
    lw   s9,    40(sp)
    lw   s10,   44(sp)
    lw   s11,   48(sp)
    addi sp, sp, thread_context_size

    ret

# a0 = stack_top
# returns a0 = saved_sp
thread_context_init:
    andi a0, a0, -16
    addi a0, a0, -64

    la   t0, thread_bootstrap
    sw   t0,  0(a0)
    sw   zero,  4(a0)
    sw   zero,  8(a0)
    sw   zero, 12(a0)
    sw   zero, 16(a0)
    sw   zero, 20(a0)
    sw   zero, 24(a0)
    sw   zero, 28(a0)
    sw   zero, 32(a0)
    sw   zero, 36(a0)
    sw   zero, 40(a0)
    sw   zero, 44(a0)
    sw   zero, 48(a0)

    ret

/* ===== END FILE: kernel\context.s ===== */

/* ===== BEGIN FILE: kernel\kernel.c ===== */
#ifdef __cplusplus
extern "C" {
#endif

#include "kernel.h"
#include "kinternal.h"
#include "klogging.h"
#include <assert.h>

static const char* TAG = "kernel";

extern void thread_context_switch_coop(uint8_t **sp_cur, uint8_t **sp_next);
extern void thread_context_set(uint8_t **sp_next);
extern void* thread_context_init(void* stack_top);

void thread_yield(threadtime_t delay) {

    corecontext_t* corecontext = getcorecontext();

    assert(corecontext->threadcount > 0);

    threadinfo_t *thread_cur = &corecontext->thread_list[corecontext->threadcur];
    size_t idx_threadnext = schedule(corecontext->thread_list, corecontext->threadcur, corecontext->threadcount);

    // add checks?

    threadinfo_t *thread_next = &corecontext->thread_list[idx_threadnext];

    LOGI(TAG, "Preempted from thread %lu to thread %lu", corecontext->threadcur, idx_threadnext);

    // Assume thread time will never overflow (make it an implementation requirement to have a sufficiently large time size)
    thread_cur->lastrun = thread_gettime();
    thread_cur->nextrun = (delay == thread_yielddelay) ? thread_cur->lastrun + yield_effective_delay_time_us : thread_cur->lastrun + delay;

    if ( corecontext->threadcur == idx_threadnext ) { return; }
    
    corecontext->threadcur = idx_threadnext;

    thread_context_switch_coop(&thread_cur->sp_cur, &thread_next->sp_cur);

}

// bootstrap and run the current thread
void thread_bootstrap() {

    corecontext_t* corecontext = getcorecontext();
    LOGI(TAG, "executing thread <%s> id=%lu", corecontext->thread_list[corecontext->threadcur].name, corecontext->threadcur);

    threadinfo_t* thread_cur = &corecontext->thread_list[corecontext->threadcur];

    // Run the thread
    thread_cur->entry(thread_cur->entry_args);
    thread_cur->active = false;
    LOGI(TAG, "thread <%s> id=%lu completed execution", thread_cur->name, corecontext->threadcur);

    size_t idx_threadnext = schedule(corecontext->thread_list, corecontext->threadcur, corecontext->threadcount);
    threadinfo_t *thread_next = &corecontext->thread_list[idx_threadnext];

    if ( corecontext->threadcur == idx_threadnext ) {
        LOGI(TAG, "Finished execution of all threads?");
        while(1) {;}
    }

    LOGI(TAG, "Preempted from thread %lu to thread %lu", corecontext->threadcur, idx_threadnext);

    corecontext->threadcur = idx_threadnext;
    thread_context_set(&thread_next->sp_cur);

}

void thread_create(thread_entrypoint_t entry, void* args, threadpriority_t priority, char* name) {

    corecontext_t* corecontext = getcorecontext();

    if ( corecontext->threadcount == MAX_THREADS ) {
        LOGE(TAG, "Failed to instantiate new thread, thread list is full!");
        return;
    }

    threadinfo_t* thread_new = &corecontext->thread_list[corecontext->threadcount++];

    thread_new->nextrun = 0;
    thread_new->lastrun = 0;
    thread_new->sp_size = THREAD_STACK_SIZE;
    thread_new->sp_cur = thread_new->stack_base + thread_new->sp_size;
    thread_new->priority = priority;
    thread_new->active = true;
    thread_new->entry = entry;
    thread_new->entry_args = args;
    thread_new->name = name;

    thread_new->sp_cur = thread_context_init(thread_new->sp_cur);

}

void thread_begin() {

    corecontext_t* context = getcorecontext();

    // run the first available thread
    for ( size_t i = 0; i < context->threadcount; i++ ) {
        if ( context->thread_list[i].active == true ) {
            context->threadcur = i;
            thread_context_set(&context->thread_list[i].sp_cur);
        }
    }

}

#ifdef __cplusplus
}
#endif

/* ===== END FILE: kernel\kernel.c ===== */

/* ===== BEGIN FILE: kernel\kimpl.c ===== */
#include "kernel.h"
#include "kinternal.h"
#include "hardware/timer.h"
#include "pico/platform.h"
#include "pico/runtime.h"
#include "klogging.h"

static const char* TAG = "kimpl";

static volatile bool libinitialized = false;
static corecontext_t __ctx_core0;
static corecontext_t __ctx_core1;

corecontext_t* getcorecontext() {
    return get_core_num() == 0 ? &__ctx_core0 : &__ctx_core1;
}

threadtime_t thread_gettime() {
    return (threadtime_t)time_us_64();
}

uint64_t thread_timetous(threadtime_t t) {
    return t;
}

uint64_t thread_timetoms(threadtime_t t) {
    return t/1000;
}

uint64_t thread_timetos(threadtime_t t) {
    return t/1000000;
}

void kernel_init_internal() {

    // initialize both core contexts
    if ( libinitialized ) { return; }
    libinitialized = true;

    corecontext_t* corecontext = getcorecontext();

    for ( size_t i = 0; i < corecontext->threadcount; i++ ) {
        corecontext->thread_list[i].active = false;
        corecontext->thread_list[i].entry = NULL;
        corecontext->thread_list[i].entry_args = NULL;       
    }

}

// Ensure the kernel internals are initialized during SDK startup (before main).
PICO_RUNTIME_INIT_FUNC(kernel_init_internal, "12000");

/* ===== END FILE: kernel\kimpl.c ===== */

/* ===== BEGIN FILE: kernel\schedule.c ===== */
#include "kinternal.h"
#include "stdint.h"
#include "stddef.h"
#include "klogging.h"

size_t schedule(threadinfo_t* threads, size_t threadcur, size_t threadcount) {

    const char* TAG = "scheduler";
    
    LOGI(TAG, "running scheduler");

    // PSTTRF - Priotitized Shortest Time To Run First scheduler
    // This scheduler behaves like a STTRF

    // priority overrides next runtime

    threadtime_t min_nextrun = thread_maxtime;
    threadpriority_t max_priority = thread_idle_priority;
    size_t nextthread_index = SIZE_MAX;

    for ( size_t i = 0; i < threadcount; i++ ) {
        if ( (!threads[i].active) || (i == threadcur) ) { continue; }

        if ( threads[i].nextrun < thread_gettime() ) { continue; }

        if ( threads[i].priority > max_priority ) {
            max_priority = threads[i].priority;
            min_nextrun = threads[i].nextrun;
            nextthread_index = i;
        } else if ( threads[i].priority == max_priority && threads[i].nextrun < min_nextrun ) {
            max_priority = threads[i].priority;
            min_nextrun = threads[i].nextrun;
            nextthread_index = i;
        }

    }

    // I suppose this isnt an issue, we could just run the previous thread right?
    if ( min_nextrun == thread_maxtime || nextthread_index == SIZE_MAX ) {
        LOGW(TAG, "failed to identify valid next thread to run!");
        nextthread_index = threadcur;
    }

    return nextthread_index;

}

/* ===== END FILE: kernel\schedule.c ===== */

/* ===== BEGIN FILE: kernel\klogging.c ===== */
#include "klogging.h"
#include "kinternal.h"

#ifdef __cplusplus
extern "C" {
#endif

void __log_printf(int level, const char* tag, const char* format, ...) {
    if (__log_level_global >= level) {
        printf("%s", ANSI_DISABLE);

        #ifdef LOG_USE_TIMESTAMP

            printf("[%llu] ", thread_timetoms(thread_gettime())); // Print timestamp in milliseconds since boot

        #endif

        #ifdef LOG_USE_COLORS
            if ( level == LOG_LEVEL_ERROR ) {
                printf("%s", ANSI_COLOR_RED);
            }
            else if ( level == LOG_LEVEL_WARN ) {
                printf("%s", ANSI_COLOR_YELLOW);
            }
            else if ( level == LOG_LEVEL_INFO ) {
                printf("%s", ANSI_COLOR_GREEN);
            }
        #endif

        printf("%s: ", tag);

        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);

        printf("%s\n", ANSI_DISABLE);
    }
}

void _test_fn(const char *fname, int result, const char *_tag) {
    int fname_end = 0;
    while (fname[fname_end] != '(' && fname[fname_end] != '\0') fname_end++;
    char log_out[fname_end + 32];
    memcpy(log_out, "Running |", 10);
    memcpy(log_out + 9, fname, fname_end);
    log_out[fname_end + 9] = '\0';
    strcat(log_out, result ? "|...[OK]" : "|...[FAIL]");
    
    if (result) {
        LOGI(_tag, "%s", log_out);
    } else {
        LOGE(_tag, "%s", log_out);
        while(1);
    }    
};

#ifdef __cplusplus
}
#endif

/* ===== END FILE: kernel\klogging.c ===== */
