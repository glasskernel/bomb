#ifndef _CM_API_H_
#define _CM_API_H_

#include <sys/types.h>

#define VIRT_TO_PHYS(_virt_addr_) ((uint64_t)(_virt_addr_))

#define CM_FLUSH_DCACHE_RANGE(addr, length) \
	clean_dcache_range((unsigned long long)(addr), \
			   (unsigned long long)((addr) + (length)))
#define CM_INV_DCACHE_RANGE(addr, length) \
	invalidate_dcache_range((unsigned long long)(addr), \
				(unsigned long long)((addr) + (length)))

#define CACHE_WRITEBACK_SHIFT	6
#define CACHE_WRITEBACK_GRANULE	(1 << CACHE_WRITEBACK_SHIFT)

#define SMC_AARCH64_PREFIX	0xC2000000
#define SMC_CM_RANDOM		0x101C

#define RV_SUCCESS		0

#define GET_RANDOM_WORD		0

uint64_t cm_get_random(uint8_t *random, uint32_t random_len);

#endif /* _CM_API_H_ */
