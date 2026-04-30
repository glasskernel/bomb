ARCH := arm64
ARM_CPU := cortex-a53
TARGET := gta4xlwifi

WITH_KERNEL_VM := 0
WITH_LINKER_GC := 1

GLOBAL_DEFINES += \
	SYSPARAM_ALLOW_WRITE=1 \
	WITH_LIB_CONSOLE=1 \
	INPUT_GPT_AS_PT=0 \
	GPT_PART=0 \
	BOOTLOADER_FB_ADDRESS=0xCA000000 \
	LCD_WIDTH=1200 \
	LCD_HEIGHT=2000 \
	POWER_TOP=0 \
	POWER_HEIGHT=200 \
	VOL_TOP=0 \
	VOL_HEIGHT=400

export INPUT_GPT_AS_PT=0
export GPT_PART=0

MODULES += \
	app/gta4xl_boot

MODULE_DEPS += \
	lib/block \
	lib/cksum \
	lib/fastboot \
	lib/fdt \
	lib/font \
	lib/gpt \
	lib/libavb \
	lib/ufdt \
	dev/adc \
	dev/interrupt/arm_gic \
	dev/rpmb \
	dev/scsi \
	dev/timer/arm_generic \
	dev/usb/device/fastboot \
	dev/usb/dwc3 \
	dev/usb/phy/exynos
