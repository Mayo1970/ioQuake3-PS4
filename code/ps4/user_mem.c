/*
 * user_mem.c - PS4 custom memory allocator using SceLibcInternal mspace
 *
 * Maps a large flexible memory region and creates an mspace, then
 * overrides malloc/free/calloc/realloc/memalign/posix_memalign globally.
 * Piglet calls malloc internally during eglGetDisplay; without a working
 * allocator backed by sufficient memory, it fails silently.
 *
 * Based on OsirizX/sm64-port ps4/memory/user_mem.c
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <orbis/_types/kernel.h>
#include "user_mem.h"

static OrbisMspace s_mspace;
static OrbisMallocManagedSize s_mmsize;
static void  *s_mem_start;
static size_t s_mem_size = USER_MEM_SIZE;

static int s_last_reserve_ret;
static int s_last_map_ret;
static size_t s_actual_size; /* size that succeeded */

int malloc_init(void)
{
	int res;

	if (s_mspace)
		return 0;

	/* ioq3 needs ~176MB minimum (128MB hunk + 48MB zone).
	 * Start at 256MB, fall back to smaller. */
	static const size_t try_sizes[] = {
		0x10000000ULL, /*  256 MB */
		0x0C000000ULL, /*  192 MB */
		0x08000000ULL, /*  128 MB */
		0
	};

	for (int i = 0; try_sizes[i] != 0; i++) {
		size_t sz = try_sizes[i];

		s_mem_start = NULL;
		res = sceKernelReserveVirtualRange(&s_mem_start, sz,
		                                   0, USER_MEM_ALIGN);
		s_last_reserve_ret = res;
		if (res < 0)
			continue;

		res = sceKernelMapNamedSystemFlexibleMemory(&s_mem_start, sz,
		                                            ORBIS_KERNEL_PROT_CPU_RW,
		                                            ORBIS_KERNEL_MAP_FIXED,
		                                            "ioq3 User Mem");
		s_last_map_ret = res;
		if (res < 0) {
			/* Unmap the virtual reservation before retrying */
			sceKernelMunmap(s_mem_start, sz);
			continue;
		}

		/* Map succeeded -- create mspace */
		s_mem_size = sz;
		s_actual_size = sz;
		s_mspace = sceLibcMspaceCreate("ioq3 Mspace", s_mem_start,
		                               s_mem_size, 0);
		if (!s_mspace)
			return 0x30;

		s_mmsize.sz  = sizeof(s_mmsize);
		s_mmsize.ver = 1;
		sceLibcMspaceMallocStatsFast(s_mspace, &s_mmsize);
		return 0;
	}

	/* All sizes failed */
	return s_last_map_ret < 0 ? s_last_map_ret : s_last_reserve_ret;
}

int malloc_get_debug(int *out_reserve, int *out_map, void **out_base,
                     size_t *out_size)
{
	if (out_reserve) *out_reserve = s_last_reserve_ret;
	if (out_map)     *out_map     = s_last_map_ret;
	if (out_base)    *out_base    = s_mem_start;
	if (out_size)    *out_size    = s_actual_size;
	return s_mspace ? 1 : 0;
}

int malloc_finalize(void)
{
	int res;

	if (s_mspace) {
		res = sceLibcMspaceDestroy(s_mspace);
		if (res != 0)
			return 1;
		s_mspace = NULL;
	}

	res = sceKernelReleaseFlexibleMemory(s_mem_start, s_mem_size);
	if (res < 0)
		return 1;

	res = sceKernelMunmap(s_mem_start, s_mem_size);
	if (res < 0)
		return 1;

	return 0;
}

void *malloc(size_t size)
{
	if (!s_mspace)
		malloc_init();
	return sceLibcMspaceMalloc(s_mspace, size);
}

void free(void *ptr)
{
	if (!ptr || !s_mspace)
		return;
	sceLibcMspaceFree(s_mspace, ptr);
}

void *calloc(size_t nelem, size_t size)
{
	if (!s_mspace)
		malloc_init();
	return sceLibcMspaceCalloc(s_mspace, nelem, size);
}

void *realloc(void *ptr, size_t size)
{
	if (!s_mspace)
		malloc_init();
	return sceLibcMspaceRealloc(s_mspace, ptr, size);
}

void *memalign(size_t boundary, size_t size)
{
	if (!s_mspace)
		malloc_init();
	return sceLibcMspaceMemalign(s_mspace, boundary, size);
}

int posix_memalign(void **ptr, size_t boundary, size_t size)
{
	if (!s_mspace)
		malloc_init();
	return sceLibcMspacePosixMemalign(s_mspace, ptr, boundary, size);
}

void *reallocalign(void *ptr, size_t size, size_t boundary)
{
	if (!s_mspace)
		malloc_init();
	return sceLibcMspaceReallocalign(s_mspace, ptr, boundary, size);
}

int malloc_stats(OrbisMallocManagedSize *mmsize)
{
	if (!s_mspace)
		malloc_init();
	return sceLibcMspaceMallocStats(s_mspace, mmsize);
}

int malloc_stats_fast(OrbisMallocManagedSize *mmsize)
{
	if (!s_mspace)
		malloc_init();
	return sceLibcMspaceMallocStatsFast(s_mspace, mmsize);
}

size_t malloc_usable_size(void *ptr)
{
	if (!ptr)
		return 0;
	return sceLibcMspaceMallocUsableSize(ptr);
}

int vasprintf(char **bufp, const char *format, va_list ap)
{
	va_list ap1;
	int bytes;

	va_copy(ap1, ap);
	bytes = vsnprintf(NULL, 0, format, ap1) + 1;
	va_end(ap1);

	*bufp = malloc(bytes);
	if (!*bufp)
		return -1;

	return vsnprintf(*bufp, bytes, format, ap);
}

int asprintf(char **bufp, const char *format, ...)
{
	va_list ap;
	int rv;

	va_start(ap, format);
	rv = vasprintf(bufp, format, ap);
	va_end(ap);

	return rv;
}

char *strdup(const char *s)
{
	size_t len = strlen(s) + 1;
	void *p = malloc(len);
	if (!p)
		return NULL;
	return (char *)memcpy(p, s, len);
}

char *strndup(const char *s, size_t n)
{
	size_t len;
	char *result;

	if (!s)
		return NULL;

	len = strnlen(s, n);
	result = malloc(len + 1);
	if (!result)
		return NULL;

	result[len] = '\0';
	return (char *)memcpy(result, s, len);
}

void get_user_mem_size(size_t *max_mem, size_t *cur_mem)
{
	s_mmsize.sz  = sizeof(s_mmsize);
	s_mmsize.ver = 1;
	sceLibcMspaceMallocStatsFast(s_mspace, &s_mmsize);
	*max_mem += s_mmsize.curSysSz;
	*cur_mem += s_mmsize.curSysSz - s_mmsize.curUseSz;
}
