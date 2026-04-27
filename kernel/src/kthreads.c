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

tid_t treadid_counter = 1;

volatile bool libinitialized_core0 = false;
volatile bool libinitialized_core1 = false;

corecontext_t __ctx_core0 = { {}, 0, 0 };
corecontext_t __ctx_core1 = { {}, 0, 0 };

uint32_t __irqstate_core0;
uint32_t __irqstate_core1;

// --------------------------------------------------
// Preemptive task switching and state handling
// --------------------------------------------------

extern void thread_tick_handler();
extern void thread_context_load(uint8_t* sp) __attribute((noreturn));
extern void thread_context_switch() __attribute((noreturn));
extern void* thread_context_init(uint8_t* stack_top);

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

    // attach tick interrupt

    riscv_timer_set_mtimecmp(riscv_timer_get_mtime() + KERNEL_TICK_TIME_US);
    riscv_timer_set_enabled(true);

    irq_set_riscv_vector_handler(RISCV_VEC_MACHINE_TIMER_IRQ, thread_tick_handler);
    riscv_set_csr(mie, 1u << 7); // enable interrupts

}

void kernel_tick_handle() {
    riscv_timer_set_mtimecmp(riscv_timer_get_mtimecmp() + KERNEL_TICK_TIME_US);
}

void kernel_critical_enter() {

    // TODO:
    // There needs to be some sort of check to see if interrupts have already been disabled on this core
    // also some sort of safe reporting/logging mechanism

    uint32_t irqstate = save_and_disable_interrupts();
    if ( get_core_num() == 0 ) { __irqstate_core0 = irqstate; }
    else { __irqstate_core1 = irqstate; }

}

void kernel_critical_exit() {

    // TODO:
    // This needs to check if the stored interrupt state is valid

    uint32_t irqstate = get_core_num() == 0 ? __irqstate_core0 : __irqstate_core1;
    restore_interrupts_from_disabled(irqstate);

}

uint8_t* thread_preempt_schedule(uint8_t* sp_cur) {

    kernel_critical_enter();

    // LOGI(TAG, "running preemptive scheduler");

    corecontext_t* corecontext = getcorecontext();

    corecontext->thread_list[corecontext->threadcur].lastrun = thread_gettime();
    corecontext->thread_list[corecontext->threadcur].nextrun = thread_gettime();

    size_t next_task = schedule(corecontext->thread_list, corecontext->threadcur, corecontext->threadcount);

    corecontext->thread_list[corecontext->threadcur].sp_cur = sp_cur;

    corecontext->threadcur = next_task;

    kernel_critical_exit();

    return corecontext->thread_list[next_task].sp_cur;

}

// bootstrap and run the current thread
void thread_bootstrap() {

    corecontext_t* corecontext = getcorecontext();
    LOGI(TAG, "executing thread <%s> id=%lu", corecontext->thread_list[corecontext->threadcur].name, corecontext->threadcur);

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

    kernel_critical_enter();

    corecontext_t* corecontext = getcorecontext();

    assert(corecontext->threadcount > 0);

    threadinfo_t *thread_cur = &corecontext->thread_list[corecontext->threadcur];

    // Assume thread time will never overflow (make it an implementation requirement to have a sufficiently large time size)
    thread_cur->lastrun = thread_gettime();
    thread_cur->nextrun = (delay == thread_yielddelay) ? thread_cur->lastrun + yield_effective_delay_time_us : thread_cur->lastrun + delay;

    thread_context_switch();

}

void thread_create(thread_entrypoint_t entry, void* args, threadpriority_t priority, const char* name) {

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
    thread_new->tid = treadid_counter++;

    thread_new->sp_cur = thread_context_init(thread_new->sp_cur);

}

void thread_begin() {

    corecontext_t* context = getcorecontext();
    kernel_threading_init_internal();

    // run the first available thread
    for ( size_t i = 0; i < context->threadcount; i++ ) {
        if ( context->thread_list[i].active == true ) {
            context->threadcur = i;
            thread_context_load(context->thread_list[i].sp_cur);
            return;
        }
    }

}

#ifdef __cplusplus
}
#endif
