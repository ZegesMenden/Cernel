#ifdef __cplusplus
extern "C" {
#endif

#include "kthreads.h"
#include "kinternal.h"
#include "hardware/timer.h"
#include "pico/platform.h"
#include "pico/runtime.h"
#include "hardware/sync.h"
#include "klogging.h"

#include "hardware/hazard3.h"
#include "hardware/riscv.h"
#include "hardware/riscv_platform_timer.h"
#include "hardware/irq.h"

static const char* TAG = "kernel";

#ifndef KTHREAD_TRACE_ENABLE
#define KTHREAD_TRACE_ENABLE 1
#endif

#ifndef KTHREAD_TRACE_TICK_ENABLE
#define KTHREAD_TRACE_TICK_ENABLE 0
#endif

#if KTHREAD_TRACE_ENABLE
#define KTRACE(...) LOGI(TAG, __VA_ARGS__)
#else
#define KTRACE(...) ((void)0)
#endif

tid_t threadid_counter = TID_FIRST_VALID;

volatile bool libinitialized_core0 = false;
volatile bool libinitialized_core1 = false;

corecontext_t __ctx_core0 = { {}, 0, 0 };
corecontext_t __ctx_core1 = { {}, 0, 0 };

// --------------------------------------------------
// Preemptive task switching and state handling
// --------------------------------------------------

extern void thread_tick_handler();
extern void thread_context_load(uint8_t* sp) __attribute((noreturn));
extern void thread_context_switch();
extern void* thread_context_init(uint8_t* stack_top);

static uint32_t __boot_critical_irqstate_core0 = 0;
static uint32_t __boot_critical_irqstate_core1 = 0;
static uint __boot_critical_depth_core0 = 0;
static uint __boot_critical_depth_core1 = 0;

static threadinfo_t* get_current_threadinfo() {
    corecontext_t* corecontext = getcorecontext();
    if (corecontext->threadcount == 0 || corecontext->threadcur >= corecontext->threadcount) {
        return NULL;
    }

    return &corecontext->thread_list[corecontext->threadcur];
}

corecontext_t* getcorecontext() {
    return get_core_num() == 0 ? &__ctx_core0 : &__ctx_core1;
}

tid_t get_calling_tid() {
    corecontext_t* context = getcorecontext();
    return context->thread_list[context->threadcur].tid;
}

// Thread timing functions

threadtime_t thread_gettime() { return (threadtime_t)time_us_64(); }

uint64_t thread_timetous(threadtime_t t) { return t; }

uint64_t thread_timetoms(threadtime_t t) { return t/1000; }

uint64_t thread_timetos(threadtime_t t) {
    return t/1000000;
}

threadtime_t thread_ustotime(uint64_t t_us) {
    return t_us;
}
threadtime_t thread_mstotime(uint64_t t_ms) {
    return t_ms*1000;
}
threadtime_t thread_stotime(uint64_t t_s) {
    return t_s*1000000;
}

bool context_is_initialized() { return get_core_num() == 0 ? libinitialized_core0 : libinitialized_core1; }

void kernel_threading_init_internal() {

    // initialize both core contexts
    if ( context_is_initialized() ) { return; }
    if ( get_core_num() == 0 ) { libinitialized_core0 = true; }
    else { libinitialized_core1 = true; }

    KTRACE("threading init core=%u", (unsigned)get_core_num());

    // attach tick interrupt

    // riscv_timer_set_mtimecmp(riscv_timer_get_mtime() + KERNEL_TICK_TIME_US);
    // riscv_timer_set_enabled(true);

    // irq_set_riscv_vector_handler(RISCV_VEC_MACHINE_TIMER_IRQ, thread_tick_handler);
    // riscv_set_csr(mie, 1u << 7); // enable interrupts

}

void kernel_tick_handle() {
    riscv_timer_set_mtimecmp(riscv_timer_get_mtimecmp() + KERNEL_TICK_TIME_US);
#if KTHREAD_TRACE_ENABLE && KTHREAD_TRACE_TICK_ENABLE
    KTRACE("tick core=%u", (unsigned)get_core_num());
#endif
}

void kernel_critical_enter() {

    uint core_num = get_core_num();
    threadinfo_t* thread_cur = get_current_threadinfo();
    if (thread_cur == NULL) {
        uint* boot_depth = core_num == 0 ? &__boot_critical_depth_core0 : &__boot_critical_depth_core1;
        uint32_t* boot_irqstate = core_num == 0 ? &__boot_critical_irqstate_core0 : &__boot_critical_irqstate_core1;

        if (*boot_depth == 0) {
            *boot_irqstate = save_and_disable_interrupts();
        }

        (*boot_depth)++;
        return;
    }

    if (thread_cur->critical_depth == 0) {
        thread_cur->critical_irqstate = save_and_disable_interrupts();
    }

    thread_cur->critical_depth++;

}

void kernel_critical_exit() {

    uint core_num = get_core_num();
    threadinfo_t* thread_cur = get_current_threadinfo();
    if (thread_cur == NULL) {
        uint* boot_depth = core_num == 0 ? &__boot_critical_depth_core0 : &__boot_critical_depth_core1;
        uint32_t* boot_irqstate = core_num == 0 ? &__boot_critical_irqstate_core0 : &__boot_critical_irqstate_core1;

        if (*boot_depth == 0) {
            LOGE(TAG, "(boot context) kernel_critical_exit without matching enter");
            return;
        }

        (*boot_depth)--;
        if (*boot_depth == 0) {
            restore_interrupts_from_disabled(*boot_irqstate);
        }
        return;
    }

    if (thread_cur->critical_depth == 0) {
        LOGE(TAG, "(tid=%llu) kernel_critical_exit without matching enter", (unsigned long long)thread_cur->tid);
        return;
    }

    thread_cur->critical_depth--;
    if (thread_cur->critical_depth == 0) {
        restore_interrupts_from_disabled(thread_cur->critical_irqstate);
    }

}

uint8_t* thread_preempt_schedule(uint8_t* sp_cur) {

    // LOGI(TAG, "running preemptive scheduler");

    corecontext_t* corecontext = getcorecontext();
    size_t current_task = corecontext->threadcur;
    threadinfo_t* current_thread = &corecontext->thread_list[current_task];

    // Safe-switch rule: do not preempt while the current thread is in a critical section.
    if (current_thread->critical_depth > 0) {
        return sp_cur;
    }

    KTRACE("preempt_enter core=%u cur_idx=%lu cur_tid=%llu sp_in=%p next_run=%llu depth=%lu",
        (unsigned)get_core_num(),
        (unsigned long)current_task,
        (unsigned long long)current_thread->tid,
        sp_cur,
        (unsigned long long)current_thread->nextrun,
        (unsigned long)current_thread->critical_depth);

    corecontext->thread_list[current_task].lastrun = thread_gettime();
    corecontext->thread_list[current_task].nextrun = thread_gettime();

    size_t next_task = schedule(corecontext->thread_list, current_task, corecontext->threadcount);
    threadinfo_t* next_thread = &corecontext->thread_list[next_task];

    KTRACE("preempt_sched core=%u cur_idx=%lu -> next_idx=%lu next_tid=%llu next_active=%u next_run=%llu sp_next=%p",
        (unsigned)get_core_num(),
        (unsigned long)current_task,
        (unsigned long)next_task,
        (unsigned long long)next_thread->tid,
        (unsigned)next_thread->active,
        (unsigned long long)next_thread->nextrun,
        next_thread->sp_cur);

    corecontext->thread_list[current_task].sp_cur = sp_cur;

    corecontext->threadcur = next_task;

    KTRACE("preempt_pick core=%u cur_idx=%lu cur_tid=%llu -> next_idx=%lu next_tid=%llu sp_out=%p",
        (unsigned)get_core_num(),
        (unsigned long)current_task,
        (unsigned long long)current_thread->tid,
        (unsigned long)next_task,
        (unsigned long long)next_thread->tid,
        next_thread->sp_cur);

    return corecontext->thread_list[next_task].sp_cur;

}

// bootstrap and run the current thread
void thread_bootstrap() {

    corecontext_t* corecontext = getcorecontext();
    LOGI(TAG, "executing thread <%s> idx=%lu id=%llu", corecontext->thread_list[corecontext->threadcur].name, corecontext->threadcur, corecontext->thread_list[corecontext->threadcur].tid);

    threadinfo_t* thread_cur = &corecontext->thread_list[corecontext->threadcur];

    // Run the thread
    thread_cur->entry(thread_cur->entry_args);
    thread_cur->active = false;
    
    kernel_memory_thread_exit(thread_cur->tid);

    LOGI(TAG, "thread <%s> id=%lu completed execution", thread_cur->name, corecontext->threadcur);

    size_t idx_threadnext = schedule(corecontext->thread_list, corecontext->threadcur, corecontext->threadcount);
    threadinfo_t *thread_next = &corecontext->thread_list[idx_threadnext];

    if ( corecontext->threadcur == idx_threadnext ) {
        LOGI(TAG, "Finished execution of all threads?");
        while(1) {;}
    }

    LOGI(TAG, "Preempted from thread %lu to thread %lu", corecontext->threadcur, idx_threadnext);

    corecontext->threadcur = idx_threadnext;
    thread_context_load(thread_next->sp_cur);

}

// External threading functions

void thread_delay(threadtime_t delay) {

    // kernel_critical_enter();

    corecontext_t* corecontext = getcorecontext();

    assert(corecontext->threadcount > 0);

    threadinfo_t *thread_cur = &corecontext->thread_list[corecontext->threadcur];
    threadtime_t now = thread_gettime();

    KTRACE("delay_enter core=%u idx=%lu tid=%llu now=%llu delay=%llu depth=%lu",
        (unsigned)get_core_num(),
        (unsigned long)corecontext->threadcur,
        (unsigned long long)thread_cur->tid,
        (unsigned long long)now,
        (unsigned long long)delay,
        (unsigned long)thread_cur->critical_depth);

    // Assume thread time will never overflow (make it an implementation requirement to have a sufficiently large time size)
    thread_cur->lastrun = now;
    thread_cur->nextrun = (delay == thread_yielddelay) ? thread_cur->lastrun + yield_effective_delay_time_us : thread_cur->lastrun + delay;

        KTRACE("delay_switch core=%u idx=%lu tid=%llu lastrun=%llu nextrun=%llu",
            (unsigned)get_core_num(),
            (unsigned long)corecontext->threadcur,
            (unsigned long long)thread_cur->tid,
            (unsigned long long)thread_cur->lastrun,
            (unsigned long long)thread_cur->nextrun);

    // Safe-switch rule: cooperative switch is only allowed outside critical sections.
    if (thread_cur->critical_depth == 0) {
        thread_context_switch();
    } else {
        KTRACE("delay_skip_switch core=%u idx=%lu tid=%llu depth=%lu",
               (unsigned)get_core_num(),
               (unsigned long)corecontext->threadcur,
               (unsigned long long)thread_cur->tid,
               (unsigned long)thread_cur->critical_depth);
    }

    KTRACE("delay_resume core=%u idx=%lu tid=%llu now=%llu depth=%lu",
        (unsigned)get_core_num(),
        (unsigned long)corecontext->threadcur,
        (unsigned long long)get_calling_tid(),
        (unsigned long long)thread_gettime(),
        (unsigned long)thread_cur->critical_depth);

    // kernel_critical_exit();

}

void thread_create(thread_entrypoint_t entry, void* args, threadpriority_t priority, const char* name) {

    corecontext_t* corecontext = getcorecontext();

    kernel_critical_enter();

    if ( corecontext->threadcount == MAX_THREADS ) {
        LOGE(TAG, "Failed to instantiate new thread, thread list is full!");
        kernel_critical_exit();
        return;
    }

    size_t slot = corecontext->threadcount;
    threadinfo_t* thread_new = &corecontext->thread_list[slot];

    thread_new->nextrun = 0;
    thread_new->lastrun = 0;
    thread_new->sp_size = THREAD_STACK_SIZE;
    thread_new->sp_cur = thread_new->stack_base + thread_new->sp_size;
    thread_new->priority = priority;
    thread_new->active = true;
    thread_new->entry = entry;
    thread_new->entry_args = args;
    thread_new->name = name;
    thread_new->tid = threadid_counter++;
    thread_new->critical_irqstate = 0;
    thread_new->critical_depth = 0;

    thread_new->sp_cur = thread_context_init(thread_new->sp_cur);

        KTRACE("thread_create core=%u slot=%lu tid=%llu prio=%u name=%s sp=%p count=%lu",
            (unsigned)get_core_num(),
            (unsigned long)slot,
            (unsigned long long)thread_new->tid,
            (unsigned)priority,
            name == NULL ? "(null)" : name,
            thread_new->sp_cur,
            (unsigned long)corecontext->threadcount);

    kernel_critical_exit();

    corecontext->threadcount++;


}

void thread_begin() {

    corecontext_t* context = getcorecontext();
    kernel_threading_init_internal();

    LOGI(TAG, "thread library initialized");
    KTRACE("thread_begin core=%u count=%lu", (unsigned)get_core_num(), (unsigned long)context->threadcount);

    // run the first available thread
    for ( size_t i = 0; i < context->threadcount; i++ ) {
        if ( context->thread_list[i].active == true ) {
            context->threadcur = i;
            KTRACE("thread_begin_pick core=%u idx=%lu tid=%llu name=%s sp=%p",
                   (unsigned)get_core_num(),
                   (unsigned long)i,
                   (unsigned long long)context->thread_list[i].tid,
                   context->thread_list[i].name,
                   context->thread_list[i].sp_cur);
            thread_context_load(context->thread_list[i].sp_cur);
            return;
        }
    }

}

#ifdef __cplusplus
}
#endif
