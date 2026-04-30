/*
 * user_mem.h - PS4 custom memory allocator using SceLibcInternal mspace
 *
 * Overrides malloc/free/calloc/realloc globally so that both ioq3 and
 * Piglet (which calls malloc internally) allocate from a large flexible
 * memory region. Without this, SceLibcInternal's default heap is too
 * small and eglGetDisplay fails silently.
 *
 * Based on OsirizX/sm64-port ps4/memory/user_mem.c
 */
#ifndef USER_MEM_H
#define USER_MEM_H

#include <stddef.h>

#define USER_MEM_SIZE  (0xA0000000ULL)  /* 2560 MiB */
#define USER_MEM_ALIGN (16ULL * 1024)

#define ORBIS_KERNEL_MAP_FIXED  0x10

/* orbis/_types/kernel.h defines ORBIS_KERNEL_PROT_CPU_RW as VM_PROT_RW,
 * but VM_PROT_RW is not defined in the OpenOrbis headers. Force the
 * correct value (READ|WRITE = 0x01|0x02 = 0x03). */
#undef ORBIS_KERNEL_PROT_CPU_RW
#define ORBIS_KERNEL_PROT_CPU_RW 0x03

typedef void *OrbisMspace;

typedef struct OrbisMallocManagedSize {
	unsigned short sz;
	unsigned short ver;
	unsigned int   reserv;
	size_t         maxSysSz;
	size_t         curSysSz;
	size_t         maxUseSz;
	size_t         curUseSz;
} OrbisMallocManagedSize;

/* SceLibcInternal mspace API */
OrbisMspace sceLibcMspaceCreate(const char *, void *, size_t, unsigned int);
int         sceLibcMspaceDestroy(OrbisMspace);
void       *sceLibcMspaceMalloc(OrbisMspace, size_t);
void       *sceLibcMspaceCalloc(OrbisMspace, size_t, size_t);
void       *sceLibcMspaceRealloc(OrbisMspace, void *, size_t);
void       *sceLibcMspaceReallocalign(OrbisMspace, void *, size_t, size_t);
void       *sceLibcMspaceMemalign(OrbisMspace, size_t, size_t);
size_t      sceLibcMspaceMallocUsableSize(void *);
int         sceLibcMspacePosixMemalign(OrbisMspace, void **, size_t, size_t);
int         sceLibcMspaceFree(OrbisMspace, void *);
int         sceLibcMspaceMallocStats(OrbisMspace, OrbisMallocManagedSize *);
int         sceLibcMspaceMallocStatsFast(OrbisMspace, OrbisMallocManagedSize *);

/* Kernel memory API */
int sceKernelReserveVirtualRange(void **, size_t, int, size_t);
int sceKernelMapNamedSystemFlexibleMemory(void **, size_t, int, int, const char *);
int sceKernelReleaseFlexibleMemory(void *, size_t);
int sceKernelMunmap(void *, size_t);

int  malloc_init(void);
int  malloc_finalize(void);
void get_user_mem_size(size_t *max_mem, size_t *cur_mem);

#endif /* USER_MEM_H */
