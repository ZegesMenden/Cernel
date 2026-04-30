#include <stdio.h>
#include <cstdint>
#include "pico/stdlib.h"
#include "malloc/malloc.h"

extern "C" {
    #include "kthreads.h"
    #include "klogging.h"
    #include "ktstream.h"

    #include "dload/loader.h"
}

static constexpr uint64_t kStreamSinkTid = 8ULL;

void thread_memory_demo(void* args) {

    const char* TAG = "memdemo";

    LOGI(TAG, "%s entry", TAG);

    uint8_t* buffer = (uint8_t*)kmalloc(64);
    if (buffer != NULL) {
        for (size_t i = 0; i < 64; i++) {
            buffer[i] = (uint8_t)(i & 0xFFu);
        }
        LOGI(TAG, "kmalloc(64) -> %p", buffer);
    } else {
        LOGE(TAG, "kmalloc(64) failed");
    }

    uint8_t* resized = (uint8_t*)kmalloc(32);
    if (resized != NULL) {
        for (size_t i = 0; i < 32; i++) {
            resized[i] = (uint8_t)(0xA0u + (i & 0x1Fu));
        }
        resized = (uint8_t*)krealloc(resized, 96);
        if (resized != NULL) {
            LOGI(TAG, "krealloc -> %p", resized);
        } else {
            LOGE(TAG, "krealloc failed");
        }
    }

    if (buffer != NULL) {
        kfree(buffer);
    }

    if (resized != NULL) {
        kfree(resized);
    }

    void* zeroed = kcalloc(8, sizeof(uint16_t));
    if (zeroed != NULL) {
        LOGI(TAG, "kcalloc -> %p", zeroed);
        kfree(zeroed);
    }

    LOGI(TAG, "memory demo complete");
}

void thread_stream_sink(void* args) {

    const char* TAG = "stream-sink";
    LOGI(TAG, "%s entry", TAG);

    tstream_t* reader = NULL;

    while (tstream_ropen(kStreamSinkTid, &reader) != 0) {
        thread_delay(thread_mstotime(100));
        LOGI(TAG, "delaying");
    }

    LOGI(TAG, "reader opened");
    thread_delay(thread_mstotime(250));

    (void)tstream_close(&reader);
    LOGI(TAG, "reader closed");
}

void thread_stream_source(void* args) {

    const char* TAG = "stream-source";
    LOGI(TAG, "%s entry", TAG);

    tstream_t* writer = NULL;

    while (tstream_wopen(kStreamSinkTid, &writer) != 0) {
        thread_delay(thread_mstotime(50));
    }

    LOGI(TAG, "writer opened for tid=%llu", (unsigned long long)kStreamSinkTid);
    thread_delay(thread_mstotime(250));

    (void)tstream_close(&writer);
    LOGI(TAG, "writer closed");
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
        thread_delay(thread_yielddelay);
    }

}

void threadheartbeat(void* args) {

    const char* TAG = "heartbeat";

    // LOGI(TAG, "I'm alive!");

    while(1) {
        printf("hello?\n");
        // LOGI(TAG, "I'm alive!");
        thread_delay(thread_mstotime(1000));

    }

}

int main()
{
    stdio_init_all();
    // setvbuf(stdout, NULL, _IONBF, 0);
    
    printf("Hello, world!\n");
    sleep_ms(2000);

    // klog_flash_init();
    // klog_flash_boot_dump_and_clear();

    // thread_create(thread_stream_sink, NULL, (threadpriority_t)thread_idle_priority + 2, "streamsink");
    // thread_create(thread_stream_source, NULL, (threadpriority_t)thread_idle_priority + 1, "streamsource");
    // thread_create(thread_memory_demo, NULL, (threadpriority_t)thread_idle_priority + 1, "memdemo");
    thread_create(threadidle, NULL, (threadpriority_t)thread_idle_priority, "idle");
    thread_create(threadheartbeat, NULL, (threadpriority_t)thread_idle_priority + 1, "heartbeat");

    thread_begin();

    // threadheartbeat(NULL);

    while (true) {
        printf("erm...\n");
        sleep_ms(1000);
    }
}
