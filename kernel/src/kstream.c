#include "ktstream.h"
#include "kinternal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { TSTREAM_FIRST_VALID_TID = 8ULL };
enum { TSTREAM_BLOCK_SIZE = 2048u };

typedef struct tstream_pipe_t {
    struct tstream_pipe_t* next;

    tid_t tid_source;
    tid_t tid_dest;

    uint32_t reader_count;
    bool writer_open;

    uint32_t capacity;
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t used;

    unsigned char buffer[];
} tstream_pipe_t;

struct tstream_t {

    tstream_pipe_t* pipe;
    bool writer_handle;

};

static tstream_pipe_t* g_streams = NULL;

static uint32_t tstream_pipe_capacity(void) {
    if (sizeof(tstream_pipe_t) >= TSTREAM_BLOCK_SIZE) {
        return 0;
    }

    return (uint32_t)TSTREAM_BLOCK_SIZE - (uint32_t)sizeof(tstream_pipe_t);
}

static tstream_pipe_t* tstream_find_pipe(uint64_t tdest) {
    for (tstream_pipe_t* stream = g_streams; stream != NULL; stream = stream->next) {
        if (stream->tid_dest == tdest) {
            return stream;
        }
    }

    return NULL;
}

static void tstream_link_pipe(tstream_pipe_t* pipe) {
    pipe->next = g_streams;
    g_streams = pipe;
}

static void tstream_unlink_pipe(tstream_pipe_t* pipe) {
    if (pipe == NULL) {
        return;
    }

    if (g_streams == pipe) {
        g_streams = pipe->next;
        return;
    }

    for (tstream_pipe_t* current = g_streams; current != NULL; current = current->next) {
        if (current->next != pipe) {
            continue;
        }

        current->next = pipe->next;
        return;
    }
}

static tstream_t* tstream_make_handle(tstream_pipe_t* pipe, bool writer_handle) {
    void* mem = NULL;
    if (mem_block_consume_kernel(1, TID_STREAM_MEMORY, &mem) < 0 || mem == NULL) {
        return NULL;
    }

    tstream_t* handle = (tstream_t*)mem;
    if (handle == NULL) {
        (void)mem_block_return_kernel(mem);
        return NULL;
    }

    handle->pipe = pipe;
    handle->writer_handle = writer_handle;
    return handle;
}

static void tstream_destroy_handle(tstream_t* handle) {
    if (handle != NULL) {
        (void)mem_block_return_kernel(handle);
    }
}

static tstream_pipe_t* tstream_alloc_pipe(uint64_t tsource, uint64_t tdest) {
    void* mem = NULL;
    if (mem_block_consume_kernel(1, 1, &mem) < 0 || mem == NULL) {
        return NULL;
    }

    tstream_pipe_t* pipe = (tstream_pipe_t*)mem;
    pipe->next = NULL;
    pipe->tid_source = tsource;
    pipe->tid_dest = tdest;
    pipe->reader_count = 0;
    pipe->writer_open = true;
    pipe->capacity = tstream_pipe_capacity();
    pipe->read_pos = 0;
    pipe->write_pos = 0;
    pipe->used = 0;
    return pipe;
}

static void tstream_free_pipe(tstream_pipe_t* pipe) {
    if (pipe != NULL) {
        (void)mem_block_return_kernel(pipe);
    }
}

static bool tstream_pipe_open_allowed(uint64_t caller_tid, uint64_t tdest) {
    return caller_tid >= TSTREAM_FIRST_VALID_TID && tdest >= TSTREAM_FIRST_VALID_TID && caller_tid == tdest;
}

int tstream_wopen(uint64_t tdest, tstream_t **stream) {
    if (stream == NULL) {
        return -1;
    }

    *stream = NULL;

    uint64_t caller_tid = get_calling_tid();
    if (caller_tid < TSTREAM_FIRST_VALID_TID || tdest < TSTREAM_FIRST_VALID_TID || caller_tid == tdest) {
        return -1;
    }

    kernel_critical_enter();
    if (tstream_find_pipe(tdest) != NULL) {
        kernel_critical_exit();
        return -1;
    }
    kernel_critical_exit();

    tstream_pipe_t* pipe = tstream_alloc_pipe(caller_tid, tdest);
    if (pipe == NULL) {
        return -1;
    }

    kernel_critical_enter();
    if (tstream_find_pipe(tdest) != NULL) {
        kernel_critical_exit();
        tstream_free_pipe(pipe);
        return -1;
    }

    tstream_link_pipe(pipe);
    kernel_critical_exit();

    tstream_t* handle = tstream_make_handle(pipe, true);
    if (handle == NULL) {
        kernel_critical_enter();
        tstream_unlink_pipe(pipe);
        kernel_critical_exit();
        tstream_free_pipe(pipe);
        return -1;
    }

    *stream = handle;
    return 0;
}

int tstream_ropen(uint64_t tdest, tstream_t **stream) {
    if (stream == NULL) {
        return -1;
    }

    *stream = NULL;

    if (!tstream_pipe_open_allowed(get_calling_tid(), tdest)) {
        return -1;
    }

    kernel_critical_enter();
    tstream_pipe_t* pipe = tstream_find_pipe(tdest);
    if (pipe == NULL || !pipe->writer_open) {
        kernel_critical_exit();
        return -1;
    }

    pipe->reader_count++;
    kernel_critical_exit();

    tstream_t* handle = tstream_make_handle(pipe, false);
    if (handle == NULL) {
        kernel_critical_enter();
        if (pipe->reader_count > 0) {
            pipe->reader_count--;
        }
        kernel_critical_exit();
        return -1;
    }

    *stream = handle;
    return 0;
}

int tstream_close(tstream_t **stream) {
    if (stream == NULL || *stream == NULL) {
        return -1;
    }

    tstream_t* handle = *stream;
    tstream_pipe_t* pipe = handle->pipe;
    if (pipe == NULL) {
        tstream_destroy_handle(handle);
        *stream = NULL;
        return -1;
    }

    bool release_pipe = false;

    kernel_critical_enter();
    if (handle->writer_handle) {
        if (!pipe->writer_open) {
            kernel_critical_exit();
            tstream_destroy_handle(handle);
            *stream = NULL;
            return -1;
        }

        pipe->writer_open = false;
    } else {
        if (pipe->reader_count == 0) {
            kernel_critical_exit();
            tstream_destroy_handle(handle);
            *stream = NULL;
            return -1;
        }

        pipe->reader_count--;
    }

    if (!pipe->writer_open && pipe->reader_count == 0) {
        tstream_unlink_pipe(pipe);
        release_pipe = true;
    }
    kernel_critical_exit();

    tstream_destroy_handle(handle);
    if (release_pipe) {
        tstream_free_pipe(pipe);
    }

    *stream = NULL;
    return 0;
}

