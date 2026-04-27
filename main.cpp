#include <stdio.h>
#include "pico/stdlib.h"
#include "malloc/malloc.h"

extern "C" {
    #include "kthreads.h"
    #include "klogging.h"

    #include "dload/loader.h"
}

void thread0(void* args) {

    const char* TAG = "thread0";

    while(1) {

        LOGI(TAG, "running thread0!");
        thread_delay(thread_mstotime(500));

    }

}

void threadprogloader(void* args) {

    const char* TAG = "loader";

    const uint8_t* ELFFILE;

    uint8_t progmem_dynamic[1024*4];

    loader_result_t res = loader_load_elf(ELFFILE, 1024, progmem_dynamic, 1024 * 4);
    
    switch(res.status) {
        case(EL_OK): {
            LOGI(TAG, "EL_OK");
            break;
        }
        case(EL_EIO): {
            LOGE(TAG, "EL_EIO");
            break;
        }
        case(EL_ENOMEM): {
            LOGE(TAG, "EL_ENOMEM");
            break;
        }
        case(EL_NOTELF): {
            LOGE(TAG, "EL_NOTELF");
            break;
        }
        case(EL_WRONGBITS): {
            LOGE(TAG, "EL_WRONGBITS");
            break;
        }
        case(EL_WRONGENDIAN): {
            LOGE(TAG, "EL_WRONGENDIAN");
            break;
        }
        case(EL_WRONGARCH): {
            LOGE(TAG, "EL_WRONGARCH");
            break;
        }
        case(EL_WRONGOS): {
            LOGE(TAG, "EL_WRONGOS");
            break;
        }
        case(EL_NOTEXEC): {
            LOGE(TAG, "EL_NOTEXEC");
            break;
        }
        case(EL_NODYN): {
            LOGE(TAG, "EL_NODYN");
            break;
        }
        case(EL_BADREL): {
            LOGE(TAG, "EL_BADREL");
            break;
        }
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
    thread_create(threadidle, NULL, (threadpriority_t)thread_idle_priority, "idle");

    thread_begin();

    while (true) {
        printf("erm...\n");
        sleep_ms(1000);
    }
}
