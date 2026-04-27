#include "kinternal.h"
#include "kthreads.h"
#include "kmemory.h"

extern void kernel_memory_init_internal();
extern void kernel_threading_init_internal();

void __attribute__((constructor)) kernel_init_internal() {
    kernel_memory_init_internal();
    kernel_threading_init_internal();    
}
