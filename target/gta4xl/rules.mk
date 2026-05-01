LOCAL_DIR := $(GET_LOCAL_DIR)
MODULE := $(LOCAL_DIR)

PLATFORM := maestro9610

MEMBASE := 0xDE000000
MEMSIZE := 0x00F00000

BOOT_IMAGE_TEXT_OFFSET := 0x00080000
BOOT_IMAGE_FLAGS := 0x000000000000000a

CREATE_FDT_POINTER := 0
FDT_POINTER_ADDRESS := 0x00000000

BOOTLOADER_FB_ADDRESS := 0xCA000000
LCD_WIDTH := 1200
LCD_HEIGHT := 2000

MKBOOTIMG_ARGS := \
	--tags_offset 0x00000100 \
	--second_offset 0xf0000000 \
	--pagesize 2048 \
	--os_version "13.0.0" \
	--os_patch_level "2099-12" \
	--kernel_offset 0x00008000 \
	--ramdisk_offset 0x01000000 \
	--header_version 2 \
	--dtb Resources/DTBs/gta4xl-boot-dt.dtb \
	--dtb_offset 0x00000000 \
	--cmdline "LK3RD BOOT IMAGE" \
	--board "" \
	--base 0x10000000

MODULE_SRCS += \
	$(LOCAL_DIR)/target.c

include make/module.mk
