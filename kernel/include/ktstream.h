#ifndef KPIPE_H
#define KPIPE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Thread streams:
// A thread stream uses the kernel block memory allocator to store a one-way serial byte stream.
// The memory block holds metadata about the pipe (instance of the tstream_t struct), along with
// a ring buffer containing the byte stream.

// tstream_wopen opens a stream for writing to another thread, and ropen only opens a stream if the
// sending thread has opened a writing stream.

typedef struct tstream_t tstream_t;

int tstream_wopen(uint64_t tdest, tstream_t **stream);
int tstream_ropen(uint64_t tdest, tstream_t **stream);

int tstream_close(tstream_t **stream);

#ifdef __cplusplus
}
#endif

#endif // KPIPE_H
