#ifndef MMU_FUNC_H_
#define MMU_FUNC_H_

void invalidate_dcache_all(void);
void invalidate_dcache_range(unsigned long long start, unsigned long long end);
void clean_invalidate_dcache_all(void);
void clean_dcache_range(unsigned long long start, unsigned long long end);
void clean_invalidate_dcache_range(unsigned long long start,
				   unsigned long long end);
void disable_mmu_dcache(void);
void cpu_common_init(void);

#endif /* MMU_FUNC_H_ */
