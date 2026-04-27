#include "loader.h"

#include <string.h>
#include "klogging.h"

typedef struct loader_ctx {
	el_ctx elf;
	const uint8_t *image;
	size_t image_size;
	uintptr_t load_base;
	size_t load_size;
} loader_ctx_t;

static const char *TAG = "dload";

static bool loader_pread(el_ctx *ctx, void *dest, size_t nb, size_t offset)
{
	loader_ctx_t *lctx = (loader_ctx_t *)ctx;

	if (offset > lctx->image_size) {
		LOGE(TAG, "pread out-of-bounds offset=%u size=%u", (unsigned)offset, (unsigned)lctx->image_size);
		return false;
	}

	if (nb > (lctx->image_size - offset)) {
		LOGE(TAG, "pread overflow nb=%u offset=%u size=%u", (unsigned)nb, (unsigned)offset, (unsigned)lctx->image_size);
		return false;
	}

	memcpy(dest, lctx->image + offset, nb);
	return true;
}

static void *loader_alloc(el_ctx *ctx, Elf_Addr phys, Elf_Addr virt, Elf_Addr size)
{
	loader_ctx_t *lctx = (loader_ctx_t *)ctx;
	uintptr_t start = (uintptr_t)phys;
	uintptr_t base = lctx->load_base;
	uintptr_t end;
	uintptr_t limit;

	(void)virt;

	if (size > (Elf_Addr)SIZE_MAX) {
		LOGE(TAG, "segment size too large for platform: %u", (unsigned)size);
		return NULL;
	}

	end = start + (uintptr_t)size;

	if (end < start) {
		LOGE(TAG, "address overflow while allocating segment");
		return NULL;
	}

	if (start < base) {
		LOGE(TAG, "segment start below load base: start=%p base=%p", (void *)start, (void *)base);
		return NULL;
	}

	limit = base + lctx->load_size;
	if (limit < base) {
		LOGE(TAG, "load region overflow: base=%p size=%u", (void *)base, (unsigned)lctx->load_size);
		return NULL;
	}

	if (end > limit) {
		LOGE(TAG, "segment out of range: end=%p limit=%p", (void *)end, (void *)limit);
		return NULL;
	}

	return (void *)start;
}

loader_result_t loader_load_elf(
	const void *elf_image,
	size_t elf_image_size,
	void *program_memory,
	size_t program_memory_size)
{
	loader_result_t result;
	loader_ctx_t ctx;

	result.status = EL_EIO;
	result.entry_point = 0;

	if (elf_image == NULL || program_memory == NULL) {
		LOGE(TAG, "invalid arguments: elf_image=%p program_memory=%p", elf_image, program_memory);
		return result;
	}

	if (elf_image_size == 0 || program_memory_size == 0) {
		LOGE(TAG, "invalid buffer sizes: elf_image_size=%u program_memory_size=%u", (unsigned)elf_image_size, (unsigned)program_memory_size);
		return result;
	}

	memset(&ctx, 0, sizeof(ctx));
	ctx.image = (const uint8_t *)elf_image;
	ctx.image_size = elf_image_size;
	ctx.load_base = (uintptr_t)program_memory;
	ctx.load_size = program_memory_size;
	ctx.elf.pread = loader_pread;

	result.status = el_init(&ctx.elf);
	if (result.status != EL_OK) {
		LOGE(TAG, "el_init failed: status=%d", result.status);
		return result;
	}

	if ((size_t)ctx.elf.memsz > program_memory_size) {
		result.status = EL_ENOMEM;
		LOGE(TAG, "insufficient program memory: required=%u available=%u", (unsigned)ctx.elf.memsz, (unsigned)program_memory_size);
		return result;
	}

	ctx.elf.base_load_paddr = (Elf_Addr)ctx.load_base;
	ctx.elf.base_load_vaddr = (Elf_Addr)ctx.load_base;

	result.status = el_load(&ctx.elf, loader_alloc);
	if (result.status != EL_OK) {
		LOGE(TAG, "el_load failed: status=%d", result.status);
		return result;
	}

	result.status = el_relocate(&ctx.elf);
	if (result.status != EL_OK) {
		LOGE(TAG, "el_relocate failed: status=%d", result.status);
		return result;
	}

	result.entry_point = (uintptr_t)ctx.elf.base_load_vaddr + (uintptr_t)ctx.elf.ehdr.e_entry;
	LOGI(TAG, "ELF loaded successfully: entry=%p memsz=%u", (void *)result.entry_point, (unsigned)ctx.elf.memsz);
	return result;
}
