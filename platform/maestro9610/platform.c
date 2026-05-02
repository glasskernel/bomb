/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * This software is proprietary of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied or distributed, transmitted,
 * transcribed, stored in a retrieval system or translated into any human or computer language in any form by any means,
 * electronic, mechanical, manual or otherwise, or disclosed
 * to third parties without the express written permission of Samsung Electronics.
 */

#include <lk/reg.h>
#include "uart_simple.h"
#include <dev/ufs.h>
#include <dev/boot.h>
#include <dev/rpmb.h>
#include <pit.h>
#include <dev/interrupt/arm_gic.h>
#include <dev/timer/arm_generic.h>
#include <platform/interrupts.h>
#include <platform/sfr.h>
#include <platform/smc.h>
#include <platform/speedy.h>
#include <platform/pmic_s2mpu09.h>
#include <platform/if_pmic_s2mu004.h>
#include <platform/tmu.h>
#include <platform/dfd.h>
#include <platform/ldfw.h>
#include <platform/device_info.h>
#include <platform/gpio.h>

#include <lib/font_display.h>
#include <lib/logo_display.h>
#include <lk3rd/boot_reason.h>
#include <target/dpu_config.h>
#include <stdio.h>

#define ARCH_TIMER_IRQ		30

#if defined(TARGET_GTA4XL) || defined(TARGET_GTA4XLWIFI)
#define GTA4XL_EL1_L1_ENTRIES	512
#define GTA4XL_EL1_BLOCK_SIZE	0x40000000ULL
#define GTA4XL_EL1_DESC_BLOCK	0x1ULL
#define GTA4XL_EL1_DESC_AF	(1ULL << 10)
#define GTA4XL_EL1_DESC_SH_INNER	(3ULL << 8)
#define GTA4XL_EL1_DESC_UXN	(1ULL << 54)
#define GTA4XL_EL1_DESC_PXN	(1ULL << 53)
#define GTA4XL_EL1_DESC_ATTR(n)	((unsigned long long)(n) << 2)
#define GTA4XL_EL1_DESC_DEVICE	(GTA4XL_EL1_DESC_BLOCK | GTA4XL_EL1_DESC_AF | \
				 GTA4XL_EL1_DESC_UXN | GTA4XL_EL1_DESC_PXN | \
				 GTA4XL_EL1_DESC_ATTR(0))
#define GTA4XL_EL1_DESC_NORMAL	(GTA4XL_EL1_DESC_BLOCK | GTA4XL_EL1_DESC_AF | \
				 GTA4XL_EL1_DESC_SH_INNER | \
				 GTA4XL_EL1_DESC_ATTR(2))
#define GTA4XL_EL1_MAIR	((0x04ULL << 8) | (0xffULL << 16))
#define GTA4XL_EL1_TCR		((2ULL << 32) | (1ULL << 23) | \
				 (3ULL << 12) | (1ULL << 10) | \
				 (1ULL << 8) | 32ULL)
#define GTA4XL_EL1_SCTLR_M	0x1ULL

static unsigned long long gta4xl_el1_l1_table[GTA4XL_EL1_L1_ENTRIES]
	__attribute__((aligned(4096)));
#endif

void speedy_gpio_init(void);
void xbootldo_gpio_init(void);
void fg_init_s2mu004(void);

unsigned int s5p_chip_id[4] = {0x0, 0x0, 0x0, 0x0};
unsigned int charger_mode = 0;
unsigned int board_id = 0;
unsigned int board_rev = 0;
static unsigned int dram_raw_info[24] = {0, 0, 0, 0};
unsigned long long dram_size_info = 0;
unsigned int secure_os_loaded = 0;
char enter_reason_c[ENTER_REASON_SIZE];
char *enter_reason = &enter_reason_c[0];
static char dram_type[16] = "UNKNOWN";
static char dram_manufacturer[20] = "UNKNOWN";
struct ram_info dram_info = {0, dram_manufacturer, dram_type};
struct ufs_device_info ufs_info = {0, (char *)"UNKNOWN UFS MANUFACTURER"};

#if defined(TARGET_GTA4XL) || defined(TARGET_GTA4XLWIFI)
static void gta4xl_clean_mmu_table(void)
{
	unsigned long long addr;
	unsigned long long end = (unsigned long long)gta4xl_el1_l1_table +
		sizeof(gta4xl_el1_l1_table);

	for (addr = (unsigned long long)gta4xl_el1_l1_table; addr < end; addr += 64)
		__asm__ volatile("dc cvac, %0" :: "r"(addr) : "memory");
}

static void gta4xl_enable_el1_identity_mmu(void)
{
	unsigned long long sctlr;
	unsigned long long attrs;
	unsigned long long base;
	unsigned int i;

	__asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
	if (sctlr & GTA4XL_EL1_SCTLR_M)
		return;

	for (i = 0; i < GTA4XL_EL1_L1_ENTRIES; i++)
		gta4xl_el1_l1_table[i] = 0;

	for (i = 0; i < 4; i++) {
		base = (unsigned long long)i * GTA4XL_EL1_BLOCK_SIZE;
		attrs = i < 2 ? GTA4XL_EL1_DESC_DEVICE : GTA4XL_EL1_DESC_NORMAL;
		gta4xl_el1_l1_table[i] = base | attrs;
	}

	/*
	 * Samsung uH/RKP panics on EL1 aborts while the EL1 MMU is off.
	 * Keep LK3rd's physical addresses valid by installing a 1:1 map before
	 * the first Exynos MMIO access.
	 */
	gta4xl_clean_mmu_table();
	__asm__ volatile(
		"dsb sy\n"
		"msr mair_el1, %0\n"
		"msr tcr_el1, %1\n"
		"msr ttbr0_el1, %2\n"
		"isb\n"
		"tlbi vmalle1\n"
		"dsb sy\n"
		"isb\n"
		:: "r"(GTA4XL_EL1_MAIR), "r"(GTA4XL_EL1_TCR),
		   "r"(gta4xl_el1_l1_table)
		: "memory");

	sctlr |= GTA4XL_EL1_SCTLR_M;
	__asm__ volatile(
		"msr sctlr_el1, %0\n"
		"isb\n"
		:: "r"(sctlr)
		: "memory");
}
#endif

unsigned int get_charger_mode(void)
{
	return charger_mode;
}

#if defined(TARGET_GTA4XL) || defined(TARGET_GTA4XLWIFI)
static void read_gta4xl_board_rev(void)
{
	struct exynos_gpio_bank *bank =
		(struct exynos_gpio_bank *)EXYNOS9610_GPG3CON;
	unsigned int rev = 0;
	int i;

	for (i = 0; i < 4; i++) {
		exynos_gpio_set_pull(bank, 2 + i, GPIO_PULL_NONE);
		exynos_gpio_cfg_pin(bank, 2 + i, GPIO_INPUT);
		rev |= (exynos_gpio_get_value(bank, 2 + i) & 0x1) << i;
	}

	board_id = CONFIG_BOARD_ID;
	board_rev = rev;
	printf("%s board id/rev: 0x%x/0x%x\n", CONFIG_BOARD_NAME,
	       board_id, board_rev);
}
#endif

static void read_chip_id(void)
{
	s5p_chip_id[0] = readl(EXYNOS9610_PRO_ID + CHIPID0_OFFSET);
	s5p_chip_id[1] = readl(EXYNOS9610_PRO_ID + CHIPID1_OFFSET) & 0xFFFF;
}

static void read_dram_info(void)
{
	char rank_num[20];
#ifdef CONFIG_EXYNOS_BOOTLOADER_DISPLAY
	unsigned int M5 = 0, M6 = 0, M7 = 0, M8 = 0;
#endif
	unsigned int tmp = 0;

	(void)rank_num;

	printf("%s %d\n", __func__, __LINE__);
	/* 1. Type */
	dram_raw_info[0] = readl(DRAM_INFO);
	tmp = dram_raw_info[0] & 0xF;
	printf("%s %d\n", __func__, __LINE__);

	switch (tmp) {
	case 0x0:
		strcpy(dram_type, "LPDDR4");
		break;
	case 0x2:
		strcpy(dram_type, "LPDDR4X");
		break;
	default:
		printf("Type None!\n");
	}

	printf("%s %d\n", __func__, __LINE__);
	/* 2. rank_num */
	dram_raw_info[0] = readl(DRAM_INFO);
	tmp = (dram_raw_info[0] >> 4) & 0xF;

	printf("%s %d\n", __func__, __LINE__);
	switch (tmp) {
	case 0x0:
		strcpy(rank_num, "1RANK");
		break;
	case 0x3:
		strcpy(rank_num, "2RANK");
		break;
	default:
		printf("Rank_num None!\n");
	}

	printf("%s %d\n", __func__, __LINE__);
	/* 3. manufacturer */
	dram_raw_info[0] = readl(DRAM_INFO);
	tmp = (dram_raw_info[0] >> 8) & 0xFF;
#ifdef CONFIG_EXYNOS_BOOTLOADER_DISPLAY
	M5 = tmp;
#endif

	printf("%s %d\n", __func__, __LINE__);
	switch (tmp) {
	case 0x01:
		strcpy(dram_manufacturer, "Samsung");
		break;
	case 0x06:
		strcpy(dram_manufacturer, "SK hynix");
		break;
	case 0xFF:
		strcpy(dram_manufacturer, "Micron");
		break;
	default:
		printf("Manufacturer None!\n");
	}

	printf("%s %d\n", __func__, __LINE__);
	dram_raw_info[1] = readl(DRAM_INFO + 0x4);
	dram_raw_info[2] = readl(DRAM_SIZE_INFO);
	dram_raw_info[3] = readl(DRAM_SIZE_INFO + 0x4);
	dram_size_info |= (unsigned long long)(dram_raw_info[2]);
	dram_size_info |= (unsigned long long)(dram_raw_info[3]) << 32;
	/* Set to GB */
	dram_size_info = dram_size_info / 1024 / 1024 / 1024;
	dram_info.ram_size = dram_size_info;

#ifdef CONFIG_EXYNOS_BOOTLOADER_DISPLAY
	M6 = dram_raw_info[1] & 0xFF;
	M7 = (dram_raw_info[1] >> 8) & 0xFF;
	M8 = (dram_raw_info[0] & 0x3) | (((dram_raw_info[0] >> 20) & 0xF) << 2) | ((dram_raw_info[0]  >> 16 & 0x3) << 6);
	print_lcd(FONT_WHITE, FONT_BLACK, "DRAM %lu GB %s %s %s M5=0x%02x M6=0x%02x M7=0x%02x M8=0x%02x",
		dram_size_info,	dram_type, rank_num, dram_manufacturer,
		M5, M6, M7, M8);
#endif
}

static void load_secure_payload(void)
{
	unsigned long ret = 0;
	unsigned int boot_dev = 0;
	unsigned int dfd_en = readl(EXYNOS9610_POWER_RESET_SEQUENCER_CONFIGURATION);
	unsigned int rst_stat = readl(EXYNOS9610_POWER_RST_STAT);

	if (*(unsigned int *)DRAM_BASE != 0xabcdef) {
		printf("Running on DRAM by TRACE32: skip load_secure_payload()\n");
	} else {
		if (is_first_boot()) {
			boot_dev = get_boot_device();

			/*
			 * In case WARM Reset/Watchdog Reset and DumpGPR is enabled,
			 * Secure payload doesn't have to be loaded.
			 */
			if (!((rst_stat & (WARM_RESET | LITTLE_WDT_RESET)) &&
				(dfd_en & EXYNOS9610_EDPCSR_DUMP_EN))) {
				ret = load_sp_image(boot_dev);
				if (ret)
					/*
					 * 0xFEED0002 : Signature check fail
					 * 0xFEED0020 : Anti-rollback check fail
					 */
					printf("Fail to load Secure Payload!! [ret = 0x%lX]\n", ret);
				else {
					printf("Secure Payload is loaded successfully!\n");
					secure_os_loaded = 1;
				}
			}

			/*
			 * If boot device is eMMC, emmc_endbootop() should be
			 * implemented because secure payload is the last image
			 * in boot partition.
			 */
			if (boot_dev == BOOT_EMMC)
				emmc_endbootop();
		} else {
			/* second_boot = 1; */
		}
	}
}

static int check_charger_connect(void)
{
	unsigned char read_pwronsrc = 0;
	unsigned int rst_stat = readl(EXYNOS9610_POWER_RST_STAT);

	if (rst_stat == PIN_RESET) {
		speedy_init();
		speedy_read(S2MPU09_PM_ADDR, S2MPU09_PM_PWRONSRC, &read_pwronsrc);

		/* Check USB or TA connected and PWRONSRC(USB)  */
		if(read_pwronsrc & ACOK)
			charger_mode = 1;
		else
			charger_mode = 0;
	} else {
		charger_mode = 0;
	}

	return 0;
}

#ifdef CONFIG_EXYNOS_BOOTLOADER_DISPLAY
extern int display_drv_init(void);
void display_panel_init(void);

static void initialize_fbs(void)
{
	memset((void *)CONFIG_DISPLAY_LOGO_BASE_ADDRESS, 0, LCD_WIDTH * LCD_HEIGHT * 4);
	memset((void *)CONFIG_DISPLAY_FONT_BASE_ADDRESS, 0, LCD_WIDTH * LCD_HEIGHT * 4);
}
#endif

void arm_generic_timer_disable(void)
{
	mask_interrupt(ARCH_TIMER_IRQ);
}

void platform_early_init(void)
{
	unsigned int rst_stat;

#if defined(TARGET_GTA4XL) || defined(TARGET_GTA4XLWIFI)
	gta4xl_enable_el1_identity_mmu();
#endif
	rst_stat = readl(EXYNOS9610_POWER_RST_STAT);

	read_chip_id();
#if defined(TARGET_GTA4XL) || defined(TARGET_GTA4XLWIFI)
	read_gta4xl_board_rev();
#endif

	speedy_gpio_init();
	xbootldo_gpio_init();
#ifdef CONFIG_EXYNOS_BOOTLOADER_DISPLAY
	display_panel_init();
	initialize_fbs();
#endif
	set_first_boot_device_info();

	if (is_first_boot() && !(rst_stat & (WARM_RESET | LITTLE_WDT_RESET)))
		muic_sw_uart();
	uart_test_function();
	printf("LK build date: %s, time: %s\n", __DATE__, __TIME__);

	arm_gic_init();
	writel(1 << 8, EXYNOS9610_MCT_G_TCON);
	arm_generic_timer_init(ARCH_TIMER_IRQ, 26000000);
}

void platform_init(void)
{
	u32 ret = 0;

	pmic_init();
	fg_init_s2mu004();
	check_charger_connect();
	display_pmic_info_s2mpu09();

#if defined(TARGET_GTA4XL) || defined(TARGET_GTA4XLWIFI)
	printf("Secure Payload already loaded by S-Boot; skip reload.\n");
#else
	load_secure_payload();
#endif

	if (get_boot_device() == BOOT_UFS) {
		ufs_alloc_memory();
		ufs_init(2);
		ret = ufs_set_configuration_descriptor();
		if (ret == 1)
			ufs_init(2);
	}
	pit_init(get_boot_device());

#ifdef CONFIG_EXYNOS_BOOTLOADER_DISPLAY
	/* If the display_drv_init function is not called before,
	 * you must use the print_lcd function.
	 */
	print_lcd(FONT_RED, FONT_BLACK, "LK Display is enabled!");
	ret = display_drv_init();
	if (ret == 0 && is_first_boot())
		show_boot_logo();

	/* If the display_drv_init function is called,
	 * you must use the print_lcd_update function.
	 */
	//print_lcd_update(FONT_BLUE, FONT_BLACK, "LK display is enabled!");
#endif
	read_dram_info();

	display_tmu_info();
	display_trip_info();
	dfd_display_reboot_reason();
	if (is_first_boot())
		debug_snapshot_fdt_init();

	if (secure_os_loaded == 1) {
		if (!init_keystorage())
			printf("keystorage: init done successfully.\n");
		else
			printf("keystorage: init failed.\n");

		if (!init_ldfws()) {
			printf("ldfw: init done successfully.\n");
		} else {
			printf("ldfw: init failed.\n");
		}

		rpmb_key_programming();
		rpmb_load_boot_table();
	}
}
