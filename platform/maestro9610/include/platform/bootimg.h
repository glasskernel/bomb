/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * This software is proprietary of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied or distributed, transmitted,
 * transcribed, stored in a retrieval system or translated into any human or computer language in any form by any means,
 * electronic, mechanical, manual or otherwise, or disclosed
 * to third parties without the express written permission of Samsung Electronics.
 */

#ifndef _BOOT_IMAGE_H_
#define _BOOT_IMAGE_H_

#include <target/board_info.h>

typedef struct boot_img_hdr boot_img_hdr;

#define BOOT_MAGIC "ANDROID!"
#define BOOT_MAGIC_SIZE 8
#define BOOT_NAME_SIZE 16
#define BOOT_ARGS_SIZE 512
#define BOOT_EXTRA_ARGS_SIZE 1024

#ifdef BOOT_IMG_HDR_V2
struct boot_img_hdr {
	uint8_t magic[BOOT_MAGIC_SIZE];
	uint32_t kernel_size;
	uint32_t kernel_addr;
	uint32_t ramdisk_size;
	uint32_t ramdisk_addr;
	uint32_t second_size;
	uint32_t second_addr;
	uint32_t tags_addr;
	uint32_t page_size;
	uint32_t header_version;
	uint32_t os_version;
	uint8_t name[BOOT_NAME_SIZE];
	uint8_t cmdline[BOOT_ARGS_SIZE];
	uint32_t id[8];
	uint8_t extra_cmdline[BOOT_EXTRA_ARGS_SIZE];
	uint32_t recovery_dtbo_size;
	uint64_t recovery_dtbo_offset;
	uint32_t header_size;
	uint32_t dtb_size;
	uint64_t dtb_addr;
} __attribute__((packed));
#else
struct boot_img_hdr {
	uint8_t magic[BOOT_MAGIC_SIZE];
	uint32_t kernel_size;
	uint32_t kernel_addr;
	uint32_t ramdisk_size;
	uint32_t ramdisk_addr;
	uint32_t second_size;
	uint32_t second_addr;
	uint32_t tags_addr;
	uint32_t page_size;
	uint32_t header_version;
	uint32_t os_version;
	uint8_t name[BOOT_NAME_SIZE];
	uint8_t cmdline[BOOT_ARGS_SIZE];
	uint32_t id[8];
	uint8_t extra_cmdline[BOOT_EXTRA_ARGS_SIZE];
	uint32_t recovery_dtbo_size;
	uint64_t recovery_dtbo_offset;
	uint32_t header_size;
} __attribute__((packed));
#endif

#endif /* _BOOT_IMAGE_H_ */
