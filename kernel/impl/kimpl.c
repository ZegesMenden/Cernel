#include "kernel.h"
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

static const char* TAG = "kimpl";

volatile bool libinitialized_core0 = false;
volatile bool libinitialized_core1 = false;

corecontext_t __ctx_core0 = { {}, 0, 0 };
corecontext_t __ctx_core1 = { {}, 0, 0 };

uint32_t __irqstate_core0;
uint32_t __irqstate_core1;

extern void thread_tick_handler();

corecontext_t* getcorecontext() {
    return get_core_num() == 0 ? &__ctx_core0 : &__ctx_core1;
}

bool context_is_initialized() { return get_core_num() == 0 ? libinitialized_core0 : libinitialized_core1; }

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

void kernel_init_internal() {

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

// Ensure the kernel internals are initialized during SDK startup (before main).
// PICO_RUNTIME_INIT_FUNC(kernel_init_internal, "ZZZZZ");

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