#include "malloc.h"

#include "kthreads.h"
#include "kmemory.h"
#include "klogging.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdalign.h>

// Compile the linker-wrap shims into exactly one TU.
#include "malloc_bind.h"

#ifndef KALLOC_MAX_ARENAS
#define KALLOC_MAX_ARENAS 16
#endif

// bit0: allocated
// bits[1..31]: size in alignment units
#define KALLOC_SECTOR_ALLOC_MASK  (uint32_t)0x1u
#define KALLOC_SECTOR_SIZE_SHIFT  1u

typedef struct {
    uint32_t raw;
} kalloc_sector_t;

typedef struct {
    uint8_t* base;
    size_t bytes;
    uint16_t blocks;
    uint32_t sector_count;
    bool in_use;
} kalloc_arena_t;

static const char* TAG = "kmalloc";

static kalloc_arena_t g_arenas[KALLOC_MAX_ARENAS];

// --- Sector helpers ---

static inline bool sector_is_allocated(kalloc_sector_t s) {
    return (s.raw & KALLOC_SECTOR_ALLOC_MASK) != 0;
}

static inline uint32_t sector_size_units(kalloc_sector_t s) {
    return (uint32_t)(s.raw >> KALLOC_SECTOR_SIZE_SHIFT);
}

static inline void sector_set(kalloc_sector_t* s, bool allocated, uint32_t units) {
    s->raw = (uint32_t)((units << KALLOC_SECTOR_SIZE_SHIFT) | (allocated ? 1u : 0u));
}

static inline size_t kalloc_align() {
    return (size_t)alignof(max_align_t);
}

static inline size_t align_up(size_t v, size_t a) {
    return (v + (a - 1)) & ~(a - 1);
}

// --- Lock helpers ---

static inline void kalloc_lock() { kernel_critical_enter(); }
static inline void kalloc_unlock() { kernel_critical_exit(); }

// --- Arena helpers ---

static inline kalloc_sector_t* arena_sector_top(kalloc_arena_t* arena) {
    return (kalloc_sector_t*)(arena->base + arena->bytes - sizeof(kalloc_sector_t));
}

static inline kalloc_sector_t* arena_sector_at(kalloc_arena_t* arena, uint32_t idx) {
    return arena_sector_top(arena) - (ptrdiff_t)idx;
}

static size_t arena_total_data_bytes(kalloc_arena_t* arena) {
    size_t total = 0;
    size_t a = kalloc_align();
    for (uint32_t i = 0; i < arena->sector_count; i++) {
        kalloc_sector_t s = *arena_sector_at(arena, i);
        total += (size_t)sector_size_units(s) * a;
    }
    return total;
}

static uint8_t* arena_data_ptr_for_sector(kalloc_arena_t* arena, uint32_t idx) {
    size_t a = kalloc_align();
    size_t offset = 0;
    for (uint32_t i = 0; i < idx; i++) {
        kalloc_sector_t s = *arena_sector_at(arena, i);
        offset += (size_t)sector_size_units(s) * a;
    }
    return arena->base + offset;
}

static bool arena_contains_ptr(kalloc_arena_t* arena, const void* ptr) {
    uintptr_t p = (uintptr_t)ptr;
    uintptr_t b = (uintptr_t)arena->base;
    return arena->in_use && p >= b && p < (b + arena->bytes);
}

static bool arena_find_sector_for_ptr(kalloc_arena_t* arena, void* ptr, uint32_t* out_idx) {
    if (!arena_contains_ptr(arena, ptr)) {
        return false;
    }

    uintptr_t target = (uintptr_t)ptr;
    uintptr_t base = (uintptr_t)arena->base;

    size_t a = kalloc_align();
    size_t offset = 0;
    for (uint32_t i = 0; i < arena->sector_count; i++) {
        kalloc_sector_t s = *arena_sector_at(arena, i);
        size_t sz = (size_t)sector_size_units(s) * a;
        uintptr_t start = base + offset;
        uintptr_t end = start + sz;
        if (target >= start && target < end) {
            *out_idx = i;
            return true;
        }
        offset += sz;
    }

    return false;
}

static void arena_rewrite_sector_count(kalloc_arena_t* arena, uint32_t new_count) {
    arena->sector_count = new_count;
    if (arena->sector_count == 0) {
        arena->sector_count = 1;
        sector_set(arena_sector_at(arena, 0), false, 0);
    }
}

static void arena_remove_sector_at(kalloc_arena_t* arena, uint32_t idx) {
    if (arena->sector_count <= 1 || idx >= arena->sector_count) {
        return;
    }

    // Shift sectors [idx+1 .. end) upward to fill the gap.
    for (uint32_t i = idx; i + 1 < arena->sector_count; i++) {
        *arena_sector_at(arena, i) = *arena_sector_at(arena, i + 1);
    }

    // Clear the now-unused last slot.
    sector_set(arena_sector_at(arena, arena->sector_count - 1), false, 0);
    arena_rewrite_sector_count(arena, arena->sector_count - 1);
}

static void arena_shrink_free_tail(kalloc_arena_t* arena) {
    // Remove trailing free sectors to reclaim arena internal space.
    while (arena->sector_count > 1) {
        uint32_t last = arena->sector_count - 1;
        kalloc_sector_t s = *arena_sector_at(arena, last);
        if (sector_is_allocated(s) || sector_size_units(s) == 0) {
            break;
        }
        arena_remove_sector_at(arena, last);
    }

    // If only one empty sector remains, the arena is empty.
    // Caller decides whether/when to return blocks to the system.
}

static bool arena_is_empty(kalloc_arena_t* arena) {
    if (!arena->in_use || arena->sector_count != 1) {
        return false;
    }
    kalloc_sector_t s0 = *arena_sector_at(arena, 0);
    return !sector_is_allocated(s0) && sector_size_units(s0) == 0;
}

static bool arena_reserve_slot(size_t* out_slot) {
    for (size_t i = 0; i < KALLOC_MAX_ARENAS; i++) {
        if (!g_arenas[i].in_use) {
            g_arenas[i] = (kalloc_arena_t){0};
            g_arenas[i].in_use = true; // reserved
            *out_slot = i;
            return true;
        }
    }
    return false;
}

static void arena_unreserve_slot(size_t slot) {
    if (slot >= KALLOC_MAX_ARENAS) {
        return;
    }
    g_arenas[slot] = (kalloc_arena_t){0};
}

static bool arena_try_alloc_existing_best_fit(kalloc_arena_t* arena, uint32_t req_units, uint32_t* out_idx) {
    uint32_t best_idx = UINT32_MAX;
    uint32_t best_units = UINT32_MAX;

    for (uint32_t i = 0; i < arena->sector_count; i++) {
        kalloc_sector_t s = *arena_sector_at(arena, i);
        uint32_t u = sector_size_units(s);
        if (!sector_is_allocated(s) && u >= req_units && u < best_units) {
            best_units = u;
            best_idx = i;
            if (u == req_units) {
                break;
            }
        }
    }

    if (best_idx == UINT32_MAX) {
        return false;
    }

    *out_idx = best_idx;
    return true;
}

static bool arena_can_append(kalloc_arena_t* arena, size_t req_bytes) {
    if (!arena->in_use) {
        return false;
    }

    size_t data_end_offset = arena_total_data_bytes(arena);
    uint8_t* data_end = arena->base + data_end_offset;

    kalloc_sector_t* headers_bottom = arena_sector_at(arena, arena->sector_count - 1);

    // If this is the empty sentinel, the allocation can reuse the existing header.
    if (arena->sector_count == 1) {
        kalloc_sector_t s0 = *arena_sector_at(arena, 0);
        if (!sector_is_allocated(s0) && sector_size_units(s0) == 0) {
            return data_end + req_bytes <= (uint8_t*)headers_bottom;
        }
    }

    // Otherwise we need space for both the data and a new header.
    uint8_t* headers_bottom_after = (uint8_t*)(headers_bottom - 1);
    return data_end + req_bytes <= headers_bottom_after;
}

static void* arena_append_sector(kalloc_arena_t* arena, uint32_t req_units) {
    size_t a = kalloc_align();
    size_t req_bytes = (size_t)req_units * a;
    if (!arena_can_append(arena, req_bytes)) {
        return NULL;
    }

    // If arena is empty sentinel (single sector, size=0, free), reuse it.
    if (arena->sector_count == 1) {
        kalloc_sector_t s0 = *arena_sector_at(arena, 0);
        if (!sector_is_allocated(s0) && sector_size_units(s0) == 0) {
            sector_set(arena_sector_at(arena, 0), true, req_units);
            return (void*)arena->base;
        }
    }

    // Create a new sector header at the bottom.
    kalloc_sector_t* new_sector = arena_sector_at(arena, arena->sector_count);
    sector_set(new_sector, true, req_units);
    arena_rewrite_sector_count(arena, arena->sector_count + 1);

    return (void*)(arena->base + arena_total_data_bytes(arena) - req_bytes);
}

static bool arena_init_reserved(size_t slot, void* base, uint16_t blocks) {
    if (slot >= KALLOC_MAX_ARENAS || base == NULL || blocks == 0) {
        return false;
    }
    kalloc_arena_t* arena = &g_arenas[slot];
    if (!arena->in_use || arena->base != NULL) {
        return false;
    }

    arena->base = (uint8_t*)base;
    arena->blocks = blocks;
    arena->bytes = (size_t)blocks * (size_t)KMEMORY_BLOCK_SIZE;
    arena->sector_count = 1;

    // Initialize sentinel sector header at the top.
    sector_set(arena_sector_at(arena, 0), false, 0);
    return true;
}

// --- Public allocator API ---

void* kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    size_t a = kalloc_align();
    size_t aligned = align_up(size, a);
    uint32_t req_units = (uint32_t)(aligned / a);

    kalloc_lock();

    // Global best-fit across all arenas.
    kalloc_arena_t* best_arena = NULL;
    uint32_t best_idx = 0;
    uint32_t best_units = UINT32_MAX;

    for (size_t ai = 0; ai < KALLOC_MAX_ARENAS; ai++) {
        kalloc_arena_t* arena = &g_arenas[ai];
        if (!arena->in_use) {
            continue;
        }

        uint32_t idx = 0;
        if (!arena_try_alloc_existing_best_fit(arena, req_units, &idx)) {
            continue;
        }

        uint32_t u = sector_size_units(*arena_sector_at(arena, idx));
        if (u < best_units) {
            best_units = u;
            best_arena = arena;
            best_idx = idx;
            if (u == req_units) {
                break;
            }
        }
    }

    if (best_arena != NULL) {
        sector_set(arena_sector_at(best_arena, best_idx), true, sector_size_units(*arena_sector_at(best_arena, best_idx)));
        void* ptr = (void*)arena_data_ptr_for_sector(best_arena, best_idx);
        kalloc_unlock();
        return ptr;
    }

    // No existing free sector fits. Try appending to an arena with minimal remaining slack.
    kalloc_arena_t* append_arena = NULL;
    size_t append_slack = (size_t)-1;

    for (size_t ai = 0; ai < KALLOC_MAX_ARENAS; ai++) {
        kalloc_arena_t* arena = &g_arenas[ai];
        if (!arena->in_use) {
            continue;
        }

        size_t req_bytes = aligned;
        if (!arena_can_append(arena, req_bytes)) {
            continue;
        }

        // Slack after appending (bytes left between data end and header region).
        size_t data_end_offset = arena_total_data_bytes(arena);
        uint8_t* data_end = arena->base + data_end_offset;
        kalloc_sector_t* headers_bottom = arena_sector_at(arena, arena->sector_count - 1);
        uint8_t* limit = (uint8_t*)headers_bottom;

        if (!(arena->sector_count == 1 &&
              !sector_is_allocated(*arena_sector_at(arena, 0)) &&
              sector_size_units(*arena_sector_at(arena, 0)) == 0)) {
            limit = (uint8_t*)(headers_bottom - 1);
        }

        size_t after_slack = (size_t)(limit - (data_end + req_bytes));
        if (after_slack < append_slack) {
            append_slack = after_slack;
            append_arena = arena;
        }
    }

    if (append_arena != NULL) {
        void* ptr = arena_append_sector(append_arena, req_units);
        kalloc_unlock();
        return ptr;
    }

    // Need a new arena: reserve a slot while locked, then consume blocks outside the lock.
    size_t slot = (size_t)-1;
    if (!arena_reserve_slot(&slot)) {
        kalloc_unlock();
        return NULL;
    }
    kalloc_unlock();

    size_t min_arena_bytes = aligned + sizeof(kalloc_sector_t);
    uint16_t blocks = (uint16_t)((min_arena_bytes + (KMEMORY_BLOCK_SIZE - 1)) / KMEMORY_BLOCK_SIZE);
    if (blocks == 0) {
        blocks = 1;
    }

    void* base = NULL;
    if (mem_block_consume(blocks, &base) < 0 || base == NULL) {
        kalloc_lock();
        arena_unreserve_slot(slot);
        kalloc_unlock();
        return NULL;
    }

    kalloc_lock();
    if (!arena_init_reserved(slot, base, blocks)) {
        arena_unreserve_slot(slot);
        kalloc_unlock();
        (void)mem_block_return(base);
        return NULL;
    }

    void* ptr = arena_append_sector(&g_arenas[slot], req_units);
    if (!ptr) {
        // Should be extremely rare; release arena.
        arena_unreserve_slot(slot);
        kalloc_unlock();
        (void)mem_block_return(base);
        return NULL;
    }
    kalloc_unlock();
    return ptr;
}

void kfree(void* ptr) {
    if (ptr == NULL) {
        return;
    }

    void* arena_base_to_return = NULL;

    kalloc_lock();

    for (size_t ai = 0; ai < KALLOC_MAX_ARENAS; ai++) {
        kalloc_arena_t* arena = &g_arenas[ai];
        if (!arena_contains_ptr(arena, ptr)) {
            continue;
        }

        uint32_t idx = 0;
        if (!arena_find_sector_for_ptr(arena, ptr, &idx)) {
            break;
        }

        kalloc_sector_t s = *arena_sector_at(arena, idx);
        if (!sector_is_allocated(s)) {
            break;
        }

        // Mark free.
        sector_set(arena_sector_at(arena, idx), false, sector_size_units(s));

        // Coalesce with previous.
        if (idx > 0) {
            kalloc_sector_t prev = *arena_sector_at(arena, idx - 1);
            if (!sector_is_allocated(prev)) {
                uint32_t merged = sector_size_units(prev) + sector_size_units(*arena_sector_at(arena, idx));
                sector_set(arena_sector_at(arena, idx - 1), false, merged);
                arena_remove_sector_at(arena, idx);
                idx -= 1;
            }
        }

        // Coalesce with next.
        if (idx + 1 < arena->sector_count) {
            kalloc_sector_t next = *arena_sector_at(arena, idx + 1);
            if (!sector_is_allocated(next)) {
                uint32_t merged = sector_size_units(*arena_sector_at(arena, idx)) + sector_size_units(next);
                sector_set(arena_sector_at(arena, idx), false, merged);
                arena_remove_sector_at(arena, idx + 1);
            }
        }

        arena_shrink_free_tail(arena);

        if (arena_is_empty(arena)) {
            arena_base_to_return = arena->base;
            *arena = (kalloc_arena_t){0};
        }
        break;
    }

    kalloc_unlock();

    // Never call the block allocator while holding kalloc lock.
    if (arena_base_to_return != NULL) {
        (void)mem_block_return(arena_base_to_return);
    }
}

void* krealloc(void* ptr, size_t size) {
    if (ptr == NULL) {
        return kmalloc(size);
    }
    if (size == 0) {
        kfree(ptr);
        return NULL;
    }

    size_t a = kalloc_align();
    size_t aligned = align_up(size, a);

    kalloc_lock();

    for (size_t ai = 0; ai < KALLOC_MAX_ARENAS; ai++) {
        kalloc_arena_t* arena = &g_arenas[ai];
        if (!arena_contains_ptr(arena, ptr)) {
            continue;
        }

        uint32_t idx = 0;
        if (!arena_find_sector_for_ptr(arena, ptr, &idx)) {
            break;
        }

        kalloc_sector_t s = *arena_sector_at(arena, idx);
        if (!sector_is_allocated(s)) {
            break;
        }

        size_t cur_bytes = (size_t)sector_size_units(s) * a;
        if (cur_bytes >= aligned) {
            kalloc_unlock();
            return ptr;
        }

        // Need grow: allocate elsewhere, copy, free.
        kalloc_unlock();
        void* new_ptr = kmalloc(size);
        if (!new_ptr) {
            return NULL;
        }
        memcpy(new_ptr, ptr, cur_bytes);
        kfree(ptr);
        return new_ptr;
    }

    kalloc_unlock();
    return NULL;
}

void* kcalloc(size_t count, size_t size) {
    if (count == 0 || size == 0) {
        return kmalloc(0);
    }

    // overflow check
    if (SIZE_MAX / count < size) {
        return NULL;
    }

    size_t total = count * size;
    void* ptr = kmalloc(total);
    if (!ptr) {
        return NULL;
    }
    memset(ptr, 0, total);
    return ptr;
}
