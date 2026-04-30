#include "kinternal.h"
#include "stdint.h"
#include "stddef.h"
#include "klogging.h"

static const char* TAG = "scheduler";

size_t schedule(threadinfo_t* threads, size_t threadcur, size_t threadcount) {

    // PSTTRF - Priotitized Shortest Time To Run First scheduler
    // This scheduler behaves like a STTRF

    // priority overrides next runtime

    threadtime_t min_nextrun = thread_maxtime;
    threadpriority_t max_priority = thread_idle_priority;
    size_t nextthread_index = SIZE_MAX;

    for ( size_t i = 0; i < threadcount; i++ ) {
        if ( (!threads[i].active) || (i == threadcur) ) { continue; }

        if ( threads[i].nextrun > thread_gettime() ) { continue; }

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
        // LOGW(TAG, "failed to identify valid next thread to run!");
        nextthread_index = threadcur;
    }

    // LOGI(TAG, "Identified thread %s (tid=%lu) for next run", threads[nextthread_index].name, threads[nextthread_index].tid);

    return nextthread_index;

}
