#ifdef __cplusplus
extern "C" {
#endif

#include "kernel.h"
#include "kinternal.h"
#include "klogging.h"
#include <assert.h>

static const char* TAG = "kernel";

extern void thread_context_switch_coop(uint8_t **sp_cur, uint8_t **sp_next);
extern void thread_context_set(uint8_t *sp_next);
extern void* thread_context_init(void* stack_top);

void thread_yield(threadtime_t delay) {

    corecontext_t* corecontext = getcorecontext();

    assert(corecontext->threadcount > 0);

    threadinfo_t *thread_cur = &corecontext->thread_list[corecontext->threadcur];
    size_t idx_threadnext = schedule(corecontext->thread_list, corecontext->threadcur, corecontext->threadcount);

    // add checks?

    threadinfo_t *thread_next = &corecontext->thread_list[idx_threadnext];

    // LOGI(TAG, "Preempted from thread %lu to thread %lu", corecontext->threadcur, idx_threadnext);

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
    thread_context_set(thread_next->sp_cur);

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

    thread_new->sp_cur = thread_context_init(thread_new->sp_cur);

}

void thread_begin() {

    corecontext_t* context = getcorecontext();

    // run the first available thread
    for ( size_t i = 0; i < context->threadcount; i++ ) {
        if ( context->thread_list[i].active == true ) {
            context->threadcur = i;
            thread_context_set(context->thread_list[i].sp_cur);
            return;
        }
    }

}

#ifdef __cplusplus
}
#endif
