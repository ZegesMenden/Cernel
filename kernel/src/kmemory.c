#include "kthreads.h"
#include "kinternal.h"
#include "kmemory.h"
#include "klogging.h"

#include <stdint.h>
#include <stddef.h>

#define HEAP_SIZE (size_t)((uintptr_t)(&__heap_end) - (uintptr_t)(&__heap_start))

// memory block metadata storage:
// every block resident in system memory has a cooresponding blockinfo_t entry,
// even if it is just one block in a larger (multi-block) allocation.

// The size parameter stores the size of the current allocation segment.
// The size parameter is only populated on the first block of each multi-block allocation

typedef struct blockinfo {

    tid_t owner;
    uint16_t size;

} blockinfo_t;

extern char __heap_start;
extern char __heap_end;

const char* TAG = "kmemory";

blockinfo_t* block_metadata = NULL;
size_t block_metadata_len = 0;
size_t block_max_count = 0;

static bool kmemory_is_free_owner(tid_t owner) {
    return owner == TID_UNUSED_MEMORY;
}

static bool kmemory_is_reserved_kernel_owner(tid_t owner) {
    return owner > TID_UNUSED_MEMORY && owner < TID_FIRST_VALID;
}

static size_t kmemory_metadata_block_count() {
    size_t metadata_bytes = block_metadata_len * sizeof(blockinfo_t);
    return (metadata_bytes + (KMEMORY_BLOCK_SIZE - 1)) / KMEMORY_BLOCK_SIZE;
}

static uintptr_t kmemory_heap_start() { return (uintptr_t)(&__heap_start); }
static uintptr_t kmemory_heap_end() { return (uintptr_t)(&__heap_end); }

static void* kmemory_block_ptr(size_t blockindex) {
    return (void*)(kmemory_heap_start() + (blockindex * (uintptr_t)KMEMORY_BLOCK_SIZE));
}

void kernel_memory_init_internal() {

    LOGI(TAG, "%lu bytes of system RAM are present.", HEAP_SIZE);
    LOGI(TAG, "heap range: start=%p end=%p", &__heap_start, &__heap_end);

    block_metadata = (blockinfo_t*)&__heap_start;
    block_metadata_len = (HEAP_SIZE / KMEMORY_BLOCK_SIZE);
    block_max_count = block_metadata_len - kmemory_metadata_block_count();

    LOGI(TAG, "block size: %lu", (size_t)KMEMORY_BLOCK_SIZE);
    LOGI(TAG, "block max count: %lu", block_max_count);
    LOGI(TAG, "block metadata length: %lu", block_metadata_len);
    LOGI(TAG, "metadata bytes: %lu", block_metadata_len * sizeof(blockinfo_t));

    for ( size_t i = 0; i < block_metadata_len; i++ ) {
        block_metadata[i] = (blockinfo_t){TID_UNUSED_MEMORY, 0};
    }

    // Reserve the heap blocks that contain the metadata itself.
    size_t metadata_block_count = kmemory_metadata_block_count();
    LOGI(TAG, "metadata blocks: %lu", metadata_block_count);
    for ( size_t i = 0; i < metadata_block_count && i < block_metadata_len; i++ ) {
        block_metadata[i] = (blockinfo_t){(tid_t)UINT64_MAX, 0};
    }

    LOGI(TAG, "initialized system heap");

}

static int kmemory_consume_with_owner(uint16_t count, tid_t owner, void** data) {

    LOGI(TAG, "consume request: count=%u owner=%lu data_out=%p", count, (unsigned long)owner, data);

    if ( count == 0 || data == NULL ) {
        LOGW(TAG, "consume rejected: invalid args count=%u data_out=%p", count, data);
        return -1;
    }
    if ( owner == TID_UNUSED_MEMORY ) {
        LOGW(TAG, "consume rejected: owner is unused memory");
        return -1;
    }

    kernel_critical_enter();

    size_t metadata_block_count = kmemory_metadata_block_count();
    if (metadata_block_count >= block_metadata_len) {
        kernel_critical_exit();
        LOGE(TAG, "consume failed: metadata_block_count=%lu >= block_metadata_len=%lu",
             metadata_block_count, block_metadata_len);        
        return -1;
    }

    size_t usable_start = metadata_block_count;
    size_t usable_end = block_metadata_len; // exclusive
    size_t usable_count = usable_end - usable_start;
    if ((size_t)count > usable_count) {
        kernel_critical_exit();
        LOGW(TAG, "consume failed: request=%u usable_count=%lu", count, usable_count);
        return -1;
    }

    // Best-fit search: find the smallest free run that can satisfy `count`.
    size_t best_start = (size_t)-1;
    size_t best_run_len = 0;

    size_t i = usable_start;
    while (i < usable_end) {
        // Skip used blocks.
        while (i < usable_end && !kmemory_is_free_owner(block_metadata[i].owner)) {
            i++;
        }
        if (i >= usable_end) {
            break;
        }

        // Measure free run.
        size_t run_start = i;
        while (i < usable_end && kmemory_is_free_owner(block_metadata[i].owner)) {
            i++;
        }
        size_t run_len = i - run_start;

        if (run_len >= (size_t)count) {
            if (best_start == (size_t)-1 || run_len < best_run_len) {
                best_start = run_start;
                best_run_len = run_len;
                if (best_run_len == (size_t)count) {
                    break; // exact fit
                }
            }
        }
    }

    if (best_start == (size_t)-1) {
        LOGW(TAG, "consume failed: no free run for count=%u", count);
        kernel_critical_exit();
        return -1;
    }

    // Commit allocation.
    for (size_t j = 0; j < (size_t)count; j++) {
        block_metadata[best_start + j].owner = owner;
        block_metadata[best_start + j].size = 0;
    }
    block_metadata[best_start].size = count;

    *data = kmemory_block_ptr(best_start);

    LOGI(TAG, "consume ok: owner=%lu count=%u start=%lu ptr=%p run_len=%lu",
         (unsigned long)owner, count, best_start, *data, best_run_len);

    kernel_critical_exit();
    return (int)count;
}

int mem_block_consume(uint16_t count, void** data) {
    tid_t consuming_tid = get_calling_tid();
    return kmemory_consume_with_owner(count, consuming_tid, data);
}

int mem_block_consume_kernel(uint16_t count, tid_t memory_type, void** data) {
    if (!kmemory_is_reserved_kernel_owner(memory_type)) {
        return -1;
    }

    return kmemory_consume_with_owner(count, memory_type, data);
}

static int kmemory_return_with_policy(void* mem, bool allow_kernel_owner) {

    LOGI(TAG, "return request: mem=%p allow_kernel_owner=%d", mem, allow_kernel_owner ? 1 : 0);

    if (mem == NULL) {
        LOGW(TAG, "return rejected: mem is NULL");
        return -1;
    }

    uintptr_t mem_addr = (uintptr_t)mem;
    uintptr_t heap_start = kmemory_heap_start();
    uintptr_t heap_end = kmemory_heap_end();

    if (mem_addr < heap_start || mem_addr >= heap_end) {
        LOGW(TAG, "return rejected: mem=%p outside heap range", mem);
        return -1;
    }
    uintptr_t offset = mem_addr - heap_start;
    if (offset % KMEMORY_BLOCK_SIZE != 0) {
        LOGW(TAG, "return rejected: unaligned mem=%p offset=%lu", mem, (unsigned long)offset);
        return -1;
    }

    size_t index = (size_t)(offset / KMEMORY_BLOCK_SIZE);
    size_t metadata_block_count = kmemory_metadata_block_count();
    if (index < metadata_block_count || index >= block_metadata_len) {
        LOGW(TAG, "return rejected: index=%lu metadata_block_count=%lu block_metadata_len=%lu",
             index, metadata_block_count, block_metadata_len);
        return -1;
    }

    kernel_critical_enter();

    // Find allocation head.
    size_t head = index;
    while (head > metadata_block_count &&
           !kmemory_is_free_owner(block_metadata[head].owner) &&
           block_metadata[head].size == 0) {
        head--;
    }

    uint16_t alloc_size = block_metadata[head].size;
    tid_t owner = block_metadata[head].owner;
    bool owner_is_thread = owner >= TID_FIRST_VALID;
    bool owner_is_kernel = kmemory_is_reserved_kernel_owner(owner);
    if (alloc_size == 0 ||
        (!allow_kernel_owner && !owner_is_thread) ||
        (allow_kernel_owner && !owner_is_kernel)) {
        LOGW(TAG, "return rejected: head=%lu size=%u owner=%lu allow_kernel_owner=%d",
             head, alloc_size, (unsigned long)owner, allow_kernel_owner ? 1 : 0);
        kernel_critical_exit();
        return -1;
    }

    size_t freed = 0;
    for (size_t j = 0; j < alloc_size && (head + j) < block_metadata_len; j++) {
        if (block_metadata[head + j].owner != owner) {
            break;
        }
        block_metadata[head + j].owner = TID_UNUSED_MEMORY;
        block_metadata[head + j].size = 0;
        freed++;
    }

    kernel_critical_exit();
    LOGI(TAG, "return ok: head=%lu freed=%lu owner=%lu", head, freed, (unsigned long)owner);
    return freed == 0 ? -1 : (int)freed;
}

int mem_block_return_kernel(void* mem) {
    return kmemory_return_with_policy(mem, true);
}

int mem_block_return(void* mem) {
    return kmemory_return_with_policy(mem, false);
}

void kernel_memory_thread_exit(tid_t tid) {

    if (tid < TID_FIRST_VALID) {
        LOGW(TAG, "thread exit cleanup skipped: tid=%lu", (unsigned long)tid);
        return;
    }

    LOGI(TAG, "thread exit cleanup start: tid=%lu", (unsigned long)tid);

    kernel_critical_enter();

    size_t metadata_block_count = kmemory_metadata_block_count();
    size_t freed_blocks = 0;
    size_t freed_allocs = 0;
    for (size_t i = metadata_block_count; i < block_metadata_len; i++) {
        if (block_metadata[i].owner != tid) {
            continue;
        }

        // If we're at an allocation head, free the whole run in one shot.
        if (block_metadata[i].size != 0) {
            size_t alloc_size = block_metadata[i].size;
            freed_allocs++;
            for (size_t j = 0; j < alloc_size && (i + j) < block_metadata_len; j++) {
                if (block_metadata[i + j].owner != tid) {
                    break;
                }
                block_metadata[i + j].owner = TID_UNUSED_MEMORY;
                block_metadata[i + j].size = 0;
                freed_blocks++;
            }
        } else {
            block_metadata[i].owner = TID_UNUSED_MEMORY;
            block_metadata[i].size = 0;
            freed_blocks++;
        }
    }

    kernel_critical_exit();

    LOGI(TAG, "thread exit cleanup done: tid=%lu freed_blocks=%lu freed_allocs=%lu",
         (unsigned long)tid, freed_blocks, freed_allocs);

}