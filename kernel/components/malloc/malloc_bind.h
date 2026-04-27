#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/reent.h>

extern void *kmalloc(size_t size);
extern void  kfree(void *ptr);
extern void *krealloc(void *ptr, size_t size);
extern void *kcalloc(size_t count, size_t size);

void *__wrap_malloc(size_t size) {
    return kmalloc(size);
}

void __wrap_free(void *ptr) {
    kfree(ptr);
}

void *__wrap_calloc(size_t count, size_t size) {
    return kcalloc(count, size);
}

void *__wrap_realloc(void *ptr, size_t size) {
    return krealloc(ptr, size);
}

void *__wrap__malloc_r(struct _reent *r, size_t size) {
    (void)r;
    return kmalloc(size);
}

void __wrap__free_r(struct _reent *r, void *ptr) {
    (void)r;
    kfree(ptr);
}

void *__wrap__calloc_r(struct _reent *r, size_t count, size_t size) {
    (void)r;
    return kcalloc(count, size);
}

void *__wrap__realloc_r(struct _reent *r, void *ptr, size_t size) {
    (void)r;
    return krealloc(ptr, size);
}