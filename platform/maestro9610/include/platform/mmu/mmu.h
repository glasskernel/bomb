#ifndef MMU_H_
#define MMU_H_

#include <platform/mmu/mmu_func.h>
#include <sys/types.h>

static inline void CleanAndInvalidateDCache(u64 addr, u64 size)
{
	clean_invalidate_dcache_range(addr, addr + size);
}

static inline void InvalidateDCache(u64 addr, u64 size)
{
	invalidate_dcache_range(addr, addr + size);
}

static inline void CoCleanDCache(u64 addr, u64 size)
{
	clean_dcache_range(addr, addr + size);
}

#define CleanDCache CoCleanDCache
#define CleanInvalidateDCacheAll clean_invalidate_dcache_all

#endif /* MMU_H_ */
