/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * This software is proprietary of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied or distributed, transmitted,
 * transcribed, stored in a retrieval system or translated into any human or computer language in any form by any means,
 * electronic, mechanical, manual or otherwise, or disclosed
 * to third parties without the express written permission of Samsung Electronics.
 */

#include <lk/debug.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <lk/reg.h>
#include <libfdt.h>
#include <lib/bio.h>
#include <lib/console.h>
#include <part_gpt.h>
#include <dev/boot.h>
#include <dev/rpmb.h>
#include <dev/usb/gadget.h>
#include <platform/exynos9610.h>
#include <platform/smc.h>
#include <platform/sfr.h>
#include <platform/ldfw.h>
#include <platform/charger.h>
#include <platform/ab_update.h>
#include <platform/secure_boot.h>
#include <platform/sizes.h>
#include <lib/fastboot.h>
#include <platform/bootimg.h>
#include <platform/fdt.h>
#include <platform/chip_id.h>
#include <pit.h>
#include <dev/scsi.h>

/* Memory node */
#define SIZE_2GB (0x80000000)
#define MASK_1MB (0x100000 - 1)

#define BUFFER_SIZE 2048

#define REBOOT_MODE_RECOVERY	0xFF
#define REBOOT_MODE_FACTORY	0xFD

#if TARGET_GTA4XL
#define GTA4XL_FB_BASE		BOOTLOADER_FB_ADDRESS
#define GTA4XL_FB_SIZE		0x01400000
#endif

void configure_ddi_id(void);
void arm_generic_timer_disable(void);

static char cmdline[AVB_CMD_MAX_SIZE];
static char verifiedbootstate[AVB_VBS_MAX_SIZE]="androidboot.verifiedbootstate=";
static const char *boot_slot_suffix = "";
static int boot_slot_index = -1;
static int boot_uses_ab_slots;

struct bootargs_prop {
	char prop[64];
	char val[64];
};
static struct bootargs_prop prop[32] = { { {0, }, {0, } }, };
static int prop_cnt = 0;

static int bootargs_init(void)
{
	u32 i = 0;
	u32 len = 0;
	u32 cur = 0;
	u32 is_val = 0;
	char bootargs[BUFFER_SIZE];
	int len2;
	const char *np;
	int noff;
	int ret;

	ret = fdt_check_header(fdt_dtb);
	if (ret) {
		printf("libfdt fdt_check_header(): %s\n", fdt_strerror(ret));
		return ret;
	}

	noff = fdt_path_offset(fdt_dtb, "/chosen");
	if (noff >= 0) {
		np = fdt_getprop(fdt_dtb, noff, "bootargs", &len2);
		if (len2 >= 0) {
			memset(bootargs, 0, BUFFER_SIZE);
			memcpy(bootargs, np, len2);
		}
	}

	printf("\ndefault bootargs: %s\n", bootargs);

	len = strlen(bootargs);
	for (i = 0; i < len; i++) {
		if (bootargs[i] == '=') {
			prop[prop_cnt].prop[cur++] = '\0';
			is_val = 1;
			cur = 0;
		} else if (bootargs[i] == ' ') {
			prop[prop_cnt].val[cur++] = '\0';
			is_val = 0;
			cur = 0;
			prop_cnt++;
		} else {
			if (is_val)
				prop[prop_cnt].val[cur++] = bootargs[i];
			else
				prop[prop_cnt].prop[cur++] = bootargs[i];
		}
	}

	return 0;
}
static char *get_bootargs_val(const char *name)
{
       int i = 0;

       for (i = 0; i <= prop_cnt; i++) {
               if (strncmp(prop[i].prop, name, strlen(name)) == 0)
                       return prop[i].val;
       }

       return NULL;
}

static void update_val(const char *name, const char *val)
{
	int i = 0;

	for (i = 0; i <= prop_cnt; i++) {
		if (strncmp(prop[i].prop, name, strlen(name)) == 0) {
			sprintf(prop[i].val, "%s", val);
			return;
		}
	}
}

static void bootargs_update(void)
{
	int i = 0;
	int cur = 0;
	char bootargs[BUFFER_SIZE];

	memset(bootargs, 0, sizeof(bootargs));

	for (i = 0; i <= prop_cnt; i++) {
		if (0 == strlen(prop[i].val)) {
			sprintf(bootargs + cur, "%s", prop[i].prop);
			cur += strlen(prop[i].prop);
			snprintf(bootargs + cur, 2, " ");
			cur += 1;
		} else {
			sprintf(bootargs + cur, "%s=%s", prop[i].prop, prop[i].val);
			cur += strlen(prop[i].prop) + strlen(prop[i].val) + 1;
			snprintf(bootargs + cur, 2, " ");
			cur += 1;
		}
	}

	bootargs[cur] = '\0';

	printf("\nupdated bootargs: %s\n", bootargs);

	set_fdt_val("/chosen", "bootargs", bootargs);
}

static void remove_string_from_bootargs(const char *str)
{
	char bootargs[BUFFER_SIZE];
	const char *np;
	int noff;
	int bootargs_len;
	int str_len;
	int i;

	noff = fdt_path_offset(fdt_dtb, "/chosen");
	np = fdt_getprop(fdt_dtb, noff, "bootargs", &bootargs_len);

	str_len = strlen(str);

	for (i = 0; i < bootargs_len - str_len; i++)
		if(!strncmp(str, (np + i), str_len))
			break;

	memset(bootargs, 0, BUFFER_SIZE);
	memcpy(bootargs, np, i);
	memcpy(bootargs + i, np + i + str_len, bootargs_len - i - str_len);

	fdt_setprop(fdt_dtb, noff, "bootargs", bootargs, strlen(bootargs) + 1);
}

static void set_bootargs(void)
{
	bootargs_init();

	/* update_val("console", "ttySAC0,115200"); */

	bootargs_update();
}

#if TARGET_GTA4XL
static int fdt_find_node(const char *alias, const char *path, const char *compat)
{
	const char *alias_path;
	int noff;

	if (alias) {
		alias_path = fdt_get_alias(fdt_dtb, alias);
		if (alias_path) {
			noff = fdt_path_offset(fdt_dtb, alias_path);
			if (noff >= 0)
				return noff;
		}
	}

	if (path) {
		noff = fdt_path_offset(fdt_dtb, path);
		if (noff >= 0)
			return noff;
	}

	if (compat)
		return fdt_node_offset_by_compatible(fdt_dtb, -1, compat);

	return -FDT_ERR_NOTFOUND;
}

static int fdt_ensure_subnode(int parent, const char *name)
{
	int noff;

	noff = fdt_subnode_offset(fdt_dtb, parent, name);
	if (noff >= 0)
		return noff;

	return fdt_add_subnode(fdt_dtb, parent, name);
}

static uint32_t fdt_ensure_phandle(int noff)
{
	uint32_t phandle;
	uint32_t max_phandle;

	phandle = fdt_get_phandle(fdt_dtb, noff);
	if (phandle)
		return phandle;

	max_phandle = fdt_get_max_phandle(fdt_dtb);
	if (max_phandle == (uint32_t)-1)
		return 0;

	phandle = max_phandle + 1;
	if (!phandle)
		return 0;

	fdt_setprop_u32(fdt_dtb, noff, "phandle", phandle);
	fdt_setprop_u32(fdt_dtb, noff, "linux,phandle", phandle);

	return phandle;
}

static void configure_gta4xl_framebuffer(void)
{
	fdt32_t reg[3];
	fdt32_t fb_reserved[2];
	uint32_t phandle;
	int root;
	int rmem;
	int fb;
	int dsim;
	int decon;

	root = fdt_path_offset(fdt_dtb, "/");
	if (root < 0)
		return;

	rmem = fdt_ensure_subnode(root, "reserved-memory");
	if (rmem < 0) {
		printf("gta4xl: failed to create reserved-memory: %s\n",
		       fdt_strerror(rmem));
		return;
	}

	fdt_setprop_u32(fdt_dtb, rmem, "#address-cells", 2);
	fdt_setprop_u32(fdt_dtb, rmem, "#size-cells", 1);
	fdt_setprop(fdt_dtb, rmem, "ranges", NULL, 0);

	fb = fdt_subnode_offset(fdt_dtb, rmem, "framebuffer@0xCA000000");
	if (fb < 0)
		fb = fdt_ensure_subnode(rmem, "framebuffer@ca000000");
	if (fb < 0) {
		printf("gta4xl: failed to create framebuffer reserve: %s\n",
		       fdt_strerror(fb));
		return;
	}

	reg[0] = cpu_to_fdt32(0);
	reg[1] = cpu_to_fdt32(GTA4XL_FB_BASE);
	reg[2] = cpu_to_fdt32(GTA4XL_FB_SIZE);

	fdt_setprop_string(fdt_dtb, fb, "compatible", "exynos,fb_rmem");
	fdt_setprop(fdt_dtb, fb, "reg", reg, sizeof(reg));
	phandle = fdt_ensure_phandle(fb);
	if (!phandle) {
		printf("gta4xl: failed to assign framebuffer phandle\n");
		return;
	}

	dsim = fdt_find_node("dsim0", "/dsim@0x148E0000", "samsung,exynos9-dsim");
	if (dsim < 0) {
		printf("gta4xl: DSIM node not found for framebuffer handoff: %s\n",
		       fdt_strerror(dsim));
		return;
	}

	fb_reserved[0] = cpu_to_fdt32(GTA4XL_FB_BASE);
	fb_reserved[1] = cpu_to_fdt32(GTA4XL_FB_SIZE);
	fdt_setprop(fdt_dtb, dsim, "fb_reserved", fb_reserved,
		    sizeof(fb_reserved));
	fdt_setprop_u32(fdt_dtb, dsim, "memory-region", phandle);

	decon = fdt_find_node("decon0", "/decon_f@0x148B0000",
			      "samsung,exynos9-decon");
	if (decon >= 0) {
		fdt_setprop_u32(fdt_dtb, decon, "psr_mode", 0);
		fdt_setprop_u32(fdt_dtb, decon, "trig_mode", 1);
		fdt_setprop_u32(fdt_dtb, decon, "dsi_mode", 0);
	}

	printf("gta4xl: framebuffer handoff %#x+%#x\n",
	       GTA4XL_FB_BASE, GTA4XL_FB_SIZE);
}

static void configure_gta4xl_bootargs(void)
{
	char str[BUFFER_SIZE];
	const char *np;
	int noff;
	int len;

	noff = fdt_path_offset(fdt_dtb, "/chosen");
	if (noff < 0)
		return;

	np = fdt_getprop(fdt_dtb, noff, "bootargs", &len);
	if (!np)
		np = "";

	snprintf(str, BUFFER_SIZE,
		 "%s s3cfb.bootloaderfb=0x%x androidboot.hardware=exynos9610"
		 " androidboot.em.model=SM-P615 androidboot.revision=%u"
		 " androidboot.dtbo_idx=%u",
		 np, GTA4XL_FB_BASE, board_rev, dtbo_index);
	fdt_setprop(fdt_dtb, noff, "bootargs", str, strlen(str) + 1);
}
#endif

static void set_usb_serialno(void)
{
	char str[BUFFER_SIZE];
	const char *np;
	int len;
	int noff;
	unsigned long tmp_serial_id = 0;

	tmp_serial_id = ((unsigned long)s5p_chip_id[1] << 32) | (s5p_chip_id[0]);

	printf("Set USB serial number in bootargs.(%016lx)\n", tmp_serial_id);

	noff = fdt_path_offset (fdt_dtb, "/chosen");
	np = fdt_getprop(fdt_dtb, noff, "bootargs", &len);
	snprintf(str, BUFFER_SIZE, "%s androidboot.serialno=%016lx",
						np, tmp_serial_id);
	fdt_setprop(fdt_dtb, noff, "bootargs", str, strlen(str) + 1);
}

static void configure_dtb(void)
{
	char str[BUFFER_SIZE];
	u32 soc_ver = 0;
	unsigned long sec_dram_base = 0;
	unsigned int sec_dram_size = 0;
	unsigned long sec_dram_end = 0;
	unsigned long sec_pt_base = 0;
	unsigned int sec_pt_size = 0;
	unsigned long sec_pt_end = 0;
	u64 dram_size = *(u64 *)BL_SYS_INFO_DRAM_SIZE;
	struct pit_entry *ptn;
	bdev_t *dev;
	const char *name;
	unsigned int boot_dev;
	int len;
	const char *np;
	int noff;
	struct AvbOps *ops;
	bool unlock;
	struct boot_img_hdr *b_hdr = (boot_img_hdr *)BOOT_BASE;

	/* Get Secure DRAM information */
	soc_ver = exynos_smc(SMC_CMD_GET_SOC_INFO, SOC_INFO_TYPE_VERSION, 0, 0);
        if (soc_ver == SOC_INFO_VERSION(SOC_INFO_MAJOR_VERSION, SOC_INFO_MINOR_VERSION)) {
		sec_dram_base = exynos_smc(SMC_CMD_GET_SOC_INFO,
						SOC_INFO_TYPE_SEC_DRAM_BASE,
						0,
						0);
		if (sec_dram_base == (unsigned long)ERROR_INVALID_TYPE) {
			printf("get secure memory base addr error!!\n");
			while (1);
		}

		sec_dram_size = (unsigned int)exynos_smc(SMC_CMD_GET_SOC_INFO,
							SOC_INFO_TYPE_SEC_DRAM_SIZE,
							0,
							0);
		if (sec_dram_size == (unsigned int)ERROR_INVALID_TYPE) {
			printf("get secure memory size error!!\n");
			while (1);
		}
	} else {
		printf("[ERROR] el3_mon is old version. (0x%x)\n", soc_ver);
		while (1);
	}

	sec_dram_end = sec_dram_base + sec_dram_size;

	printf("SEC_DRAM_BASE[%#lx]\n", sec_dram_base);
	printf("SEC_DRAM_SIZE[%#x]\n", sec_dram_size);

	/* Get secure page table for DRM information */
	sec_pt_base = exynos_smc(SMC_DRM_GET_SOC_INFO,
					SOC_INFO_SEC_PGTBL_BASE,
					0,
					0);
	if (sec_pt_base == ERROR_DRM_INVALID_TYPE) {
		printf("[SEC_PGTBL_BASE] Invalid type\n");
		sec_pt_base = 0;
	} else if (sec_pt_base == ERROR_DRM_FW_INVALID_PARAM) {
		printf("[SEC_PGTBL_BASE] Do not support SMC for SMC_DRM_GET_SOC_INFO\n");
		sec_pt_base = 0;
	} else if (sec_pt_base == (unsigned long)ERROR_NO_DRM_FW_INITIALIZED) {
		printf("[SEC_PGTBL_BASE] DRM LDFW is not initialized\n");
		sec_pt_base = 0;
	} else if (sec_pt_base & MASK_1MB) {
		printf("[SEC_PGTBL_BASE] Not aligned with 1MB\n");
		sec_pt_base = 0;
	}

	sec_pt_size = (unsigned int)exynos_smc(SMC_DRM_GET_SOC_INFO,
						SOC_INFO_SEC_PGTBL_SIZE,
						0,
						0);
	if (sec_pt_size == ERROR_DRM_INVALID_TYPE) {
		printf("[SEC_PGTBL_SIZE] Invalid type\n");
		sec_pt_size = 0;
	} else if (sec_pt_size == ERROR_DRM_FW_INVALID_PARAM) {
		printf("[SEC_PGTBL_SIZE] Do not support SMC for SMC_DRM_GET_SOC_INFO\n");
		sec_pt_size = 0;
	} else if (sec_pt_size == (unsigned int)ERROR_NO_DRM_FW_INITIALIZED) {
		printf("[SEC_PGTBL_SIZE] DRM LDFW is not initialized\n");
		sec_pt_size = 0;
	} else if (sec_pt_base & MASK_1MB) {
		printf("[SEC_PGTBL_SIZE] Not aligned with 1MB\n");
		sec_pt_size = 0;
	}

	sec_pt_end = sec_pt_base + sec_pt_size;

	printf("SEC_PGTBL_BASE[%#lx]\n", sec_pt_base);
	printf("SEC_PGTBL_SIZE[%#x]\n", sec_pt_size);

	/* DT control code must write after this function call. */
	merge_dto_to_main_dtb();
	resize_dt(SZ_8K);
	set_usb_serialno();
#if TARGET_GTA4XL
	configure_gta4xl_framebuffer();
	configure_gta4xl_bootargs();
#endif

	if (readl(EXYNOS9610_POWER_SYSIP_DAT0) == REBOOT_MODE_RECOVERY) {
		sprintf(str, "<0x%x>", RAMDISK_BASE);
		set_fdt_val("/chosen", "linux,initrd-start", str);

		sprintf(str, "<0x%x>", RAMDISK_BASE + b_hdr->ramdisk_size);
		set_fdt_val("/chosen", "linux,initrd-end", str);
	} else if (readl(EXYNOS9610_POWER_SYSIP_DAT0) == REBOOT_MODE_FACTORY) {
		noff = fdt_path_offset (fdt_dtb, "/chosen");
		np = fdt_getprop(fdt_dtb, noff, "bootargs", &len);
		snprintf(str, BUFFER_SIZE, "%s %s", np, "androidboot.mode=factory");
		fdt_setprop(fdt_dtb, noff, "bootargs", str, strlen(str) + 1);
		printf("Enter factory mode...");
	}

	sprintf(str, "<0x%x>", ECT_BASE);
	set_fdt_val("/ect", "parameter_address", str);

	sprintf(str, "<0x%x>", ECT_SIZE);
	set_fdt_val("/ect", "parameter_size", str);

	if (get_charger_mode()) {
		noff = fdt_path_offset (fdt_dtb, "/chosen");
		np = fdt_getprop(fdt_dtb, noff, "bootargs", &len);
		snprintf(str, BUFFER_SIZE, "%s %s", np, "androidboot.mode=charger");
		fdt_setprop(fdt_dtb, noff, "bootargs", str, strlen(str) + 1);
		printf("Enter charger mode...");
	}

	/* Add booting slot */
	if (boot_uses_ab_slots) {
		noff = fdt_path_offset(fdt_dtb, "/chosen");
		np = fdt_getprop(fdt_dtb, noff, "bootargs", &len);
		snprintf(str, BUFFER_SIZE, "%s androidboot.slot_suffix=%s",
			 np, boot_slot_suffix);
		fdt_setprop(fdt_dtb, noff, "bootargs", str, strlen(str) + 1);
	}

	/* Secure memories are carved-out in case of EVT1 */
	/*
	 * 1st DRAM node
	 */
	add_dt_memory_node(DRAM_BASE,
				sec_dram_base - DRAM_BASE);
	/*
	 * 2nd DRAM node
	 */
	if (sec_pt_base && sec_pt_size) {
		add_dt_memory_node(sec_dram_end,
					sec_pt_base - sec_dram_end);
		add_dt_memory_node(sec_pt_end,
					(DRAM_BASE + SIZE_2GB)
					- sec_pt_end);
	} else {
		add_dt_memory_node(sec_dram_end,
					(DRAM_BASE + SIZE_2GB)
					- sec_dram_end);
	}

	/*
	 * 3rd DRAM node
	 */
	add_dt_memory_node(DRAM_BASE2, SIZE_2GB);
	if (dram_size == 0x180000000)
		add_dt_memory_node(0x900000000, SIZE_2GB);

	noff = fdt_path_offset(fdt_dtb, "/reserved-memory/modem_if");
	if (noff >= 0) {
		np = fdt_getprop(fdt_dtb, noff, "reg", &len);
		if (len >= 0) {
			memset(str, 0, BUFFER_SIZE);
			memcpy(str, np, len);

			boot_dev = get_boot_device();
			if (boot_dev == BOOT_UFS)
				name = "scsi0";
			else {
				printf("Boot device: 0x%x. Unsupported boot device!\n", boot_dev);
				return;
			}

			/* get modem partition info */
			dev = bio_open(name);
			ptn = pit_get_part_info("modem");
			/* load modem header */
			dev->new_read(dev, (void *)(u64)be32_to_cpu(((const u32 *)str)[1]), ptn->blkstart, 16);
			bio_close(dev);
		}
	}

	if (!(readl(EXYNOS9610_POWER_SYSIP_DAT0) == REBOOT_MODE_RECOVERY)) {
		/* set AVB args */
		get_ops_addr(&ops);
		ops->read_is_device_unlocked(ops, &unlock);
		noff = fdt_path_offset (fdt_dtb, "/chosen");
		np = fdt_getprop(fdt_dtb, noff, "bootargs", &len);
		snprintf(str, BUFFER_SIZE, "%s %s %s", np, cmdline, verifiedbootstate);
		fdt_setprop(fdt_dtb, noff, "bootargs", str, strlen(str) + 1);
	}

	if (readl(EXYNOS9610_POWER_SYSIP_DAT0) == REBOOT_MODE_RECOVERY) {
		/* Set bootargs for recovery mode */
		remove_string_from_bootargs("skip_initramfs ");
		remove_string_from_bootargs("ro init=/init ");

		noff = fdt_path_offset (fdt_dtb, "/chosen");
		np = fdt_getprop(fdt_dtb, noff, "bootargs", &len);
		snprintf(str, BUFFER_SIZE, "%s %s", np, "root=/dev/ram0");
		fdt_setprop(fdt_dtb, noff, "bootargs", str, strlen(str) + 1);
	}

	noff = fdt_path_offset (fdt_dtb, "/chosen");
	np = fdt_getprop(fdt_dtb, noff, "bootargs", &len);
	printf("\nbootargs: %s\n", np);

	resize_dt(0);
}

int cmd_scatter_load_boot(int argc, const cmd_args *argv);

int load_boot_images(void)
{
	struct pit_entry *ptn;
	boot_img_hdr *boot_hdr;
	const char *boot_part_name;
	const char *dtbo_part_name = NULL;
	int ret;
	cmd_args argv[6];

	boot_uses_ab_slots = ab_slots_available();
	if (boot_uses_ab_slots) {
		boot_slot_index = ab_current_slot();
		boot_slot_suffix = boot_slot_index ? "_b" : "_a";
		boot_part_name = boot_slot_index ? "boot_b" : "boot_a";
	} else {
		boot_slot_index = -1;
		boot_slot_suffix = "";
		boot_part_name = "boot";
	}

	ptn = pit_get_part_info(boot_part_name);
	if (ptn == 0) {
		printf("Partition '%s' does not exist\n", boot_part_name);
		return -1;
	}

	printf("Loading boot image from '%s'\n", boot_part_name);
	ret = pit_access(ptn, PIT_OP_LOAD, (u64)BOOT_BASE, 0);
	if (ret) {
		printf("Failed to load '%s': %d\n", boot_part_name, ret);
		return ret;
	}

	boot_hdr = (boot_img_hdr *)BOOT_BASE;
	if (strncmp((char *)boot_hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
		printf("Boot image magic not found in '%s'\n", boot_part_name);
		return -1;
	}

	argv[1].u = BOOT_BASE;
	argv[2].u = KERNEL_BASE;
	argv[3].u = RAMDISK_BASE;
	argv[4].u = DT_BASE;
	argv[5].u = DTBO_BASE;
	ret = cmd_scatter_load_boot(6, argv);
	if (ret)
		return ret;

	if (boot_uses_ab_slots) {
		dtbo_part_name = boot_slot_index ? "dtbo_b" : "dtbo_a";
		if (!pit_get_part_info(dtbo_part_name))
			dtbo_part_name = NULL;
	}

	if (!dtbo_part_name && pit_get_part_info("dtbo"))
		dtbo_part_name = "dtbo";

	if (dtbo_part_name) {
		ptn = pit_get_part_info(dtbo_part_name);
		printf("Loading DTBO from '%s'\n", dtbo_part_name);
		ret = pit_access(ptn, PIT_OP_LOAD, (u64)DTBO_BASE, 0);
		if (ret) {
			printf("Failed to load '%s': %d\n", dtbo_part_name, ret);
			return ret;
		}
	}

	return 0;
}

int cmd_boot(int argc, const cmd_args *argv)
{
	fdt_dtb = (struct fdt_header *)DT_BASE;
	dtbo_table = (struct dt_table_header *)DTBO_BASE;

	if (load_boot_images()) {
		printf("Boot image load failed; entering fastboot\n");
		start_usb_gadget();
		while (1) {}
	}

#if defined(CONFIG_USE_AVB20)
	avb_main(boot_slot_suffix, cmdline, verifiedbootstate);
#endif

	configure_dtb();
	configure_ddi_id();

	/*
	 * PON (Power off notification) to storage
	 *
	 * Even with its failure, subsequential operations should be executed.
	 */
	scsi_do_ssu();

	if (readl(EXYNOS9610_POWER_SYSIP_DAT0) == REBOOT_MODE_RECOVERY || readl(EXYNOS9610_POWER_SYSIP_DAT0) == REBOOT_MODE_FACTORY)
		writel(0, EXYNOS9610_POWER_SYSIP_DAT0);
	/* notify EL3 Monitor end of bootloader */
	exynos_smc(SMC_CMD_END_OF_BOOTLOADER, 0, 0, 0);

	/* before jumping to kernel. disble arch_timer */
	arm_generic_timer_disable();

	void (*kernel_entry)(int r0, int r1, int r2, int r3);

	kernel_entry = (void (*)(int, int, int, int))KERNEL_BASE;
	kernel_entry(DT_BASE, 0, 0, 0);

	return 0;
}

STATIC_COMMAND_START
STATIC_COMMAND("boot", "start kernel booting", &cmd_boot)
STATIC_COMMAND_END(boot);
