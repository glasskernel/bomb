/*
 * Initial Samsung Galaxy Tab S6 Lite LTE 2020 (gta4xl) board profile.
 */

#ifndef __BOARD_INFO_H__
#define __BOARD_INFO_H__

#define CONFIG_BOARD_NAME	"gta4xl"
#define CONFIG_BOARD_GTA4XL

/*
 * The stock gta4xl DTBOs only expose regional board variants. Keep this
 * neutral until board-id wiring is validated against a real boot chain.
 */
#define CONFIG_BOARD_ID		0x0
#define CONFIG_UFS_BOARD_TYPE	1

#define CONFIG_USE_RPMB
#define RPMB_BLOCK_PER_PARTITION	512

#define BOOT_IMG_HDR_V2
#define CONFIG_DTB_IN_BOOT
#define CONFIG_RAMDISK_IN_BOOT

#define VOLDOWN_GPIOCON	EXYNOS9610_GPA1CON
#define VOLDOWN_BIT	6
#define VOLUP_GPIOCON	EXYNOS9610_GPA1CON
#define VOLUP_BIT	5
#define POWER_GPIOCON	EXYNOS9610_GPA1CON
#define POWER_BIT	7

#endif /* __BOARD_INFO_H__ */
