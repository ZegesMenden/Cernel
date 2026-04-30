#ifndef KERNEL_MEMORY_H
#define KERNEL_MEMORY_H

#include "klogging.h"
#include "kthreads.h"

#include "stdint.h"

#define KMEMORY_BLOCK_SIZE 2048

/// @brief Consume count blocks from system memory. Memory is not guaranteed to be stored in SRAM.
/// @param count Number of blocks to consume for this resource, bytes consumed is equal to: count*KMEMORY_BLOCK_SIZE
/// @param data Pointer containing a pointer to the first byte of allocated memory 
/// @return number of blocks allocated, or -1 if memory is not available
int mem_block_consume(uint16_t count, void** data);

/// @brief Return blocks to memory. 
/// @param mem Pointer to the first byte of data in the block. If other blocks were allocated in the same call as the block being freed, they will be freed as well.
/// @return number of blocks freed, or -1 if errors were encountered
int mem_block_return(void* mem);

#endif // KERNEL_MEMORY_H
