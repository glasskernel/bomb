#include <platform/debug.h>
#include <arch/ops.h>
#include <platform/mmu/mmu_func.h>
#include <target/dpu_config.h>
#include <lk/reg.h>
#include <stdio.h>
#include <string.h>
#include "uart_simple.h"
#include "exynos_font.h"

extern unsigned int globalUartBase;

static u32 cursor_x = 0;
static u32 cursor_y = 0;
static u32 text_color = 0xFFFFFFFF; // Opaque White

#ifdef CONFIG_DISPLAY_FONT_BASE_ADDRESS
#define FB_BASE ((u32 *)(unsigned long)CONFIG_DISPLAY_FONT_BASE_ADDRESS)
#else
#define FB_BASE ((u32 *)0xee800000)
#endif

static void clear_line(u32 y)
{
	volatile u32 *fb = FB_BASE;
	if (y + FONT_Y > LCD_HEIGHT) return;

	for (u32 row = 0; row < FONT_Y; row++) {
		for (u32 col = 0; col < LCD_WIDTH; col++) {
			fb[(y + row) * LCD_WIDTH + col] = 0;
		}
	}
}

void fb_clear_screen(u32 color)
{
	volatile u32 *fb = FB_BASE;
	for (u32 i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
		fb[i] = color;
	}

	cursor_x = 0;
	cursor_y = 0;
	clear_line(0);

	arch_clean_cache_range((addr_t)fb, LCD_WIDTH * LCD_HEIGHT * 4);
}

static void putc_fb(char c)
{
	volatile u32 *fb = FB_BASE;

	if (c == '\r') {
		cursor_x = 0;
		return;
	}

	if (c == '\n') {
		cursor_x = 0;
		cursor_y += FONT_Y;
		if (cursor_y + FONT_Y > LCD_HEIGHT) cursor_y = 0;
		clear_line(cursor_y);
		return;
	}

	if (c == '\t') {
		u32 tab_width = FONT_X * 8;
		cursor_x = (cursor_x + tab_width) & ~(tab_width - 1);
		if (cursor_x + FONT_X > LCD_WIDTH) putc_fb('\n');
		return;
	}

	if (c < 32 || c > 126) return;

	const u8 *char_data = &font[(c - ' ') * FONT_Y * 2];

	for (u32 row = 0; row < FONT_Y; ++row) {
		u32 row_data = (char_data[row * 2] << 8) | char_data[row * 2 + 1];

		for (u32 col = 0; col < FONT_X; ++col) {
			if (row_data & (1 << (15 - col))) {
				u32 px = cursor_x + col;
				u32 py = cursor_y + row;
				if (px < LCD_WIDTH && py < LCD_HEIGHT) {
					fb[py * LCD_WIDTH + px] = text_color;
				}
			}
		}
	}

	cursor_x += FONT_X;
	if (cursor_x + FONT_X > LCD_WIDTH) putc_fb('\n');
}

void platform_dputc(char c)
{
#if defined(TARGET_GTA4XL) || defined(TARGET_GTA4XLWIFI)
	putc_fb(c);
	volatile u32 *fb = FB_BASE;
	arch_clean_cache_range((addr_t)(fb + cursor_y * LCD_WIDTH), FONT_Y * LCD_WIDTH * 4);
#endif

	if (globalUartBase != 0) {
		uart_simple_char_out(c);
	}
}

int platform_dgetc(char *c, bool wait)
{
	if (globalUartBase != 0) {
		uart_simple_char_in(c);
	}
	return 0;
}
