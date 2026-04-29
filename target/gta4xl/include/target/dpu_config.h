#ifndef __GTA4XL_DISPLAY_CONFIG_H__
#define __GTA4XL_DISPLAY_CONFIG_H__

/*
 * Use the framebuffer handed over by the stock boot chain for lk3rd UI.
 * Native HX83102E panel bring-up still needs a DSI command sequence, so this
 * deliberately does not enable CONFIG_EXYNOS_BOOTLOADER_DISPLAY.
 */
#define CONFIG_DISPLAY_DRAWFONT
#define CONFIG_DISPLAY_FONT_BASE_ADDRESS	0xCA000000

#define LCD_WIDTH	1200
#define LCD_HEIGHT	2000

#endif /* __GTA4XL_DISPLAY_CONFIG_H__ */
