#include <platform/cm_api.h>
#include <platform/mmu/mmu_func.h>
#include <stdio.h>

static inline uint64_t exynos_cm_smc(uint64_t *cmd, uint64_t *arg1,
				     uint64_t *arg2, uint64_t *arg3)
{
	register uint64_t reg0 __asm__("x0") = *cmd;
	register uint64_t reg1 __asm__("x1") = *arg1;
	register uint64_t reg2 __asm__("x2") = *arg2;
	register uint64_t reg3 __asm__("x3") = *arg3;

	__asm__ volatile(
		"dsb\tsy\n"
		"smc\t0\n"
		: "+r"(reg0), "+r"(reg1), "+r"(reg2), "+r"(reg3));

	*cmd = reg0;
	*arg1 = reg1;
	*arg2 = reg2;
	*arg3 = reg3;

	return reg0;
}

uint64_t cm_get_random(uint8_t *random, uint32_t random_len)
{
	uint64_t ret;
	uint64_t r0 = SMC_AARCH64_PREFIX | SMC_CM_RANDOM;
	uint64_t r1 = GET_RANDOM_WORD;
	uint64_t r2 = VIRT_TO_PHYS(random);
	uint64_t r3 = random_len;

	CM_FLUSH_DCACHE_RANGE(random, random_len);
	ret = exynos_cm_smc(&r0, &r1, &r2, &r3);
	CM_INV_DCACHE_RANGE(random, random_len);

	if (ret != RV_SUCCESS)
		printf("[CM] get_random failed: 0x%llx\n", ret);

	return ret;
}
