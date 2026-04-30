/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * This software is proprietary of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied or distributed, transmitted,
 * transcribed, stored in a retrieval system or translated into any human or computer language in any form by any means,
 * electronic, mechanical, manual or otherwise, or disclosed
 * to third parties without the express written permission of Samsung Electronics.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <platform/bootimg.h>
#include <lib/console.h>
#include <libfdt.h>

#if defined(TARGET_GTA4XL) || defined(TARGET_GTA4XLWIFI)
#include <dev/usb/gadget.h>
#include <libdeflate.h>
#include <lk3rd/boot_reason.h>
#endif

#if defined(TARGET_GTA4XL) || defined(TARGET_GTA4XLWIFI)
#define BAIL_TO_FASTBOOT(reason) do { \
	enter_reason = (char *)(reason); \
	start_usb_gadget(); \
	while (1) {} \
} while (0)

static int consider_gzip_decompression(const void *kernel_boot_addr,
				       void *kernel_addr, void *ramdisk_addr,
				       size_t kernel_size)
{
	const uint8_t *kernel_data = (const uint8_t *)kernel_boot_addr;
	struct libdeflate_decompressor *d;
	enum libdeflate_result res;
	size_t actual_out_size = 0;

	if (kernel_data[0] != 0x1f || kernel_data[1] != 0x8b ||
	    kernel_data[2] != 0x08)
		return 1;

	d = libdeflate_alloc_decompressor();
	if (!d)
		BAIL_TO_FASTBOOT("could not allocate decompressor");

	res = libdeflate_gzip_decompress(d, kernel_boot_addr, kernel_size,
					 kernel_addr, ramdisk_addr - kernel_addr,
					 &actual_out_size);
	if (res != LIBDEFLATE_SUCCESS)
		BAIL_TO_FASTBOOT("failed to decompress kernel image");

	printf("decompressed kernel size: 0x%08zx\n", actual_out_size);
	libdeflate_free_decompressor(d);

	return 0;
}
#endif

int cmd_scatter_load_boot(int argc, const cmd_args *argv)
{
	unsigned long boot_addr, kernel_addr, dtb_addr, ramdisk_addr, recovery_dtbo_addr;
	struct boot_img_hdr *b_hdr;
	int kernel_offset;
	int dtb_offset;
	int ramdisk_offset;
	int recovery_dtbo_offset;
	int second_stage_offset;

	if (argc != 6) goto usage;

	boot_addr = argv[1].u;
	kernel_addr = argv[2].u;
	ramdisk_addr = argv[3].u;
	dtb_addr = argv[4].u;
	recovery_dtbo_addr = argv[5].u;

	b_hdr = (boot_img_hdr *)boot_addr;

	printf("page size: 0x%08x\n", b_hdr->page_size);
	printf("kernel size: 0x%08x\n", b_hdr->kernel_size);
	printf("ramdisk size: 0x%08x\n", b_hdr->ramdisk_size);
#ifdef BOOT_IMG_HDR_V2
	printf("DTB size: 0x%08x\n", b_hdr->dtb_size);
#else
	printf("DTB size: 0x%08x\n", b_hdr->second_size);
#endif
	printf("recovery DTBO size: 0x%08x\n", b_hdr->recovery_dtbo_size);

	kernel_offset = b_hdr->page_size;
	ramdisk_offset = kernel_offset + ((b_hdr->kernel_size + b_hdr->page_size - 1) / b_hdr->page_size) * b_hdr->page_size;
	second_stage_offset = ramdisk_offset + ((b_hdr->ramdisk_size + b_hdr->page_size - 1) / b_hdr->page_size) * b_hdr->page_size;
	recovery_dtbo_offset = second_stage_offset + ((b_hdr->second_size + b_hdr->page_size - 1) / b_hdr->page_size) * b_hdr->page_size;
#ifdef BOOT_IMG_HDR_V2
	dtb_offset = recovery_dtbo_offset + ((b_hdr->recovery_dtbo_size + b_hdr->page_size - 1) / b_hdr->page_size) * b_hdr->page_size;
#else
	dtb_offset = second_stage_offset;
#endif

	if (fdt_check_header((const void *)(boot_addr + dtb_offset)))
		dtb_offset = dtb_offset + 0x40;

	if (kernel_addr) {
#if defined(TARGET_GTA4XL) || defined(TARGET_GTA4XLWIFI)
		if (ramdisk_addr &&
		    consider_gzip_decompression((const void *)(boot_addr + kernel_offset),
						(void *)kernel_addr,
						(void *)ramdisk_addr,
						(size_t)b_hdr->kernel_size) == 0)
			goto kernel_done;
#endif
		memcpy((void *)kernel_addr, (const void *)(boot_addr + kernel_offset), (size_t)b_hdr->kernel_size);
	}
#if defined(TARGET_GTA4XL) || defined(TARGET_GTA4XLWIFI)
kernel_done:
#endif
	if (ramdisk_addr)
		memcpy((void *)ramdisk_addr, (const void *)(boot_addr + ramdisk_offset), (size_t)b_hdr->ramdisk_size);
	if (dtb_addr)
#ifdef BOOT_IMG_HDR_V2
		memcpy((void *)dtb_addr, (const void *)(boot_addr + dtb_offset), (size_t)b_hdr->dtb_size);
#else
		memcpy((void *)dtb_addr, (const void *)(boot_addr + dtb_offset), (size_t)b_hdr->second_size);
#endif
	if (recovery_dtbo_addr)
		memcpy((void *)recovery_dtbo_addr, (const void *)(boot_addr + recovery_dtbo_offset), (size_t)b_hdr->recovery_dtbo_size);

	return 0;

usage:
	printf("scatter_load_boot {boot/recovery addr} {kernel addr} {ramdisk addr} {dtb addr} {recovery dtbo addr}\n");
	return -1;
}

STATIC_COMMAND_START
STATIC_COMMAND("scatter_load_boot", "scatter load kernel, ramdisk, dtb, recovery dtbo from boot/recovery.img", &cmd_scatter_load_boot)
STATIC_COMMAND_END(scatter_load_boot);
