#ifndef MALLOC_H
#define MALLOC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Kernel-backed allocator API (used by malloc_bind wrappers).
void* kmalloc(size_t size);
void  kfree(void* ptr);
void* krealloc(void* ptr, size_t size);
void* kcalloc(size_t count, size_t size);

#ifdef __cplusplus
}
#endif

#endif
