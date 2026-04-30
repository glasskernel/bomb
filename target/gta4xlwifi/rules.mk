LOCAL_DIR := $(GET_LOCAL_DIR)
MODULE := $(LOCAL_DIR)

PLATFORM := maestro9610

MEMBASE := 0xFF000000
MEMSIZE := 0x00F00000

CREATE_FDT_POINTER := 0
FDT_POINTER_ADDRESS := 0x00000000

BOOTLOADER_FB_ADDRESS := 0xCA000000
LCD_WIDTH := 1200
LCD_HEIGHT := 2000

MKBOOTIMG_ARGS := \
	--tags_offset 0x00000100 \
	--pagesize 2048 \
	--os_version "13.0.0" \
	--os_patch_level "2099-12" \
	--kernel_offset 0x00080000 \
	--ramdisk_offset 0x04000000 \
	--header_version 2 \
	--cmdline "LK3RD BOOT IMAGE" \
	--board "" \
	--base 0x80000000

MODULE_SRCS += \
	$(LOCAL_DIR)/target.c

include make/module.mk
