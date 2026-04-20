#ifndef DLOAD_LOADER_H
#define DLOAD_LOADER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "elfload.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct loader_result {
	el_status status;
	uintptr_t entry_point;
} loader_result_t;

/*
 * Load an ELF image from memory into caller-provided program memory.
 * The destination memory must be writable and executable by the platform.
 */
loader_result_t loader_load_elf(
	const void *elf_image,
	size_t elf_image_size,
	void *program_memory,
	size_t program_memory_size);

#ifdef __cplusplus
}
#endif

#endif
