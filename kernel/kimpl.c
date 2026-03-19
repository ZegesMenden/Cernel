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
