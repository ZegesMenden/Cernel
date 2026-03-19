#include <stdio.h>
#include "pico/stdlib.h"

extern "C" {
    #include "kernel.h"
    #include "klogging.h"
}

void thread0(void* args) {

    const char* TAG = "thread0";
    
    while(1) {
        LOGI(TAG, "this is a message from thread 0!");

        threadtime_t t_preyield = thread_gettime();
        thread_yield(thread_ustotime(1000000));
        threadtime_t t_postyield = thread_gettime();

        LOGI(TAG, "I was yielded for %llu uS", thread_timetous(t_postyield - t_preyield));
    }

}


void thread1(void* args) {

    const char* TAG = "thread1";
    
    while(1) {
        LOGI(TAG, "this is a message from thread 1!");
        
        threadtime_t t_preyield = thread_gettime();
        thread_yield(thread_ustotime(500000));
        threadtime_t t_postyield = thread_gettime();

        LOGI(TAG, "I was yielded for %llu uS", thread_timetous(t_postyield - t_preyield));
    }

}


void thread2(void* args) {

    const char* TAG = "thread2";
    
    while(1) {
        LOGI(TAG, "this is a message from thread 2!");
        
        threadtime_t t_preyield = thread_gettime();
        thread_yield(thread_ustotime(100000));
        threadtime_t t_postyield = thread_gettime();

        LOGI(TAG, "I was yielded for %llu uS", thread_timetous(t_postyield - t_preyield));
    }

}

void threadidle(void* args) {

    while(1) {
        thread_yield(0);
    }

}

int main()
{
    stdio_init_all();

    getchar();

    thread_create(thread0, NULL, (threadpriority_t)thread_idle_priority + 1, "task0");
    thread_create(thread1, NULL, (threadpriority_t)thread_idle_priority + 1, "task1");
    thread_create(thread2, NULL, (threadpriority_t)thread_idle_priority + 1, "task2");
    thread_create(threadidle, NULL, (threadpriority_t)thread_idle_priority + 1, "idle");

    thread_begin();

    while (true) {
        printf("erm...\n");
        sleep_ms(1000);
    }
}
