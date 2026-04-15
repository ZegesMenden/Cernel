#include <stdio.h>
#include "pico/stdlib.h"

extern "C" {
    #include "kernel.h"
    #include "klogging.h"
}

void thread0(void* args) {

    const char* TAG = "thread0";

    while(1) {

        LOGI(TAG, "running thread0!");
        thread_delay(thread_mstotime(500));

    }

}

void threadpi(void* args) {

    const char* TAG = "threadpi";

    uint64_t num_inside = 0;
    uint64_t num_iters = 0;
    long double pi_est = 0;

    auto prng = [](uint32_t& seed) {
        seed = seed * 1103515245 + 12345;
        return (seed / 65536) % 32768;
    };

    uint32_t seed = thread_timetous(thread_gettime());

    while(1) {

        // if a = pi * r^2 and r = 1, then the area of a r=1 circle should be pi

        long double x = ((prng(seed) / float(32768)) * 2.0) - 1.0;
        long double y = ((prng(seed) / float(32768)) * 2.0) - 1.0;

        num_inside += ( (x * x) + (y * y) < (long double)1.0 );
        num_iters += 1;

        pi_est = 4.0 * ((long double)num_inside / (long double)num_iters);

        kernel_critical_enter();
        if ( num_iters % 10000 == 0 ) {
            LOGI(TAG, "newest pi estimate: %.12f", (float)pi_est);
        }
        kernel_critical_exit();

    }

}

void threade(void* args) {

    const char* TAG = "threadeuler";

    uint64_t num_inside = 0;
    uint64_t num_iters = 0;
    long double e_est = 0;

    auto prng = [](uint32_t& seed) {
        seed = seed * 1103515245 + 12345;
        return (seed / 65536) % 32768;
    };

    uint32_t seed = thread_timetous(thread_gettime());

    while(1) {

        // if a = pi * r^2 and r = 1, then the area of a r=1 circle should be pi

        long double x = ((prng(seed) / float(32768)) * 2) + 1;
        long double y = (prng(seed) / float(32768));

        num_inside += ( y < (1.0/x) && x < e_est );
        num_iters += 1;

        e_est = ((long double)num_inside / (long double)num_iters);

        kernel_critical_enter();
        if ( num_iters % 10000 == 0 ) {
            LOGI(TAG, "newest e estimate: %f", (float)e_est);
        }
        kernel_critical_exit();

    }

}

void threadidle(void* args) {

    while(1) {
        thread_delay(0);
    }

}

int main()
{
    stdio_init_all();

    getchar();

    thread_create(thread0, NULL, (threadpriority_t)thread_idle_priority + 1, "task0");
    thread_create(threadpi, NULL, (threadpriority_t)thread_idle_priority + 1, "taskpi");
    thread_create(threade, NULL, (threadpriority_t)thread_idle_priority + 1, "taskeuler");
    thread_create(threadidle, NULL, (threadpriority_t)thread_idle_priority, "idle");

    thread_begin();

    while (true) {
        printf("erm...\n");
        sleep_ms(1000);
    }
}
