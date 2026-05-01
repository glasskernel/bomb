/*
 * Minimal gta4xl family boot policy.
 *
 * This keeps the first Exynos 9611 bring-up path small: volume-down enters
 * fastboot, otherwise the existing 9610 boot command is used.
 */

#include <app.h>
#include <dev/usb/gadget.h>
#include <lib/console.h>
#include <lk3rd/boot_reason.h>
#include <platform/delay.h>
#include <lk/reg.h>
#include <platform/fastboot.h>
#include <platform/gpio.h>
#include <platform/if_pmic_s2mu004.h>
#include <platform/sfr.h>
#include <target/board_info.h>
#include <stdio.h>

int cmd_boot(int argc, const cmd_args *argv);

void target_init_for_usb(void)
{
	muic_sw_usb();
}

int do_fastboot(int argc, const cmd_args *argv)
{
	enter_reason = (char *)"fastboot command";
	start_usb_gadget();
	return 0;
}

int boot_fb_continue(void)
{
	return cmd_boot(0, 0);
}

int boot_fb_boot(unsigned long buf_addr, size_t size)
{
	printf("fastboot boot is not wired for %s yet\n", CONFIG_BOARD_NAME);
	return -1;
}

void mainline_boot_fb_boot(unsigned long buf_addr, size_t size)
{
	printf("fastboot mainline boot is not wired for %s yet\n",
	       CONFIG_BOARD_NAME);
}

int debug_store_ramdump_oem(const char *cmd)
{
	return -1;
}

void debug_store_ramdump_getvar(const char *cmd, char *response)
{
	*response = '\0';
}

int dss_getvar_item(const char *name, char *response)
{
	return -1;
}

static bool fastboot_key_pressed(void)
{
	struct exynos_gpio_bank *bank =
		(struct exynos_gpio_bank *)VOLDOWN_GPIOCON;

	exynos_gpio_set_pull(bank, VOLDOWN_BIT, GPIO_PULL_UP);
	exynos_gpio_cfg_pin(bank, VOLDOWN_BIT, GPIO_INPUT);
	mdelay(50);

	return exynos_gpio_get_value(bank, VOLDOWN_BIT) == 0;
}

static void gta4xl_boot_task(const struct app_descriptor *app, void *args)
{
	if (readl(EXYNOS_POWER_SYSIP_DAT0) == REBOOT_MODE_FASTBOOT_USER) {
		writel(0, EXYNOS_POWER_SYSIP_DAT0);
		enter_reason = (char *)"fastboot request via PMU DAT0";
		start_usb_gadget();
		return;
	}

	if (fastboot_key_pressed()) {
		printf("Entering fastboot: volume down pressed\n");
		enter_reason = (char *)"volume down pressed";
		start_usb_gadget();
		return;
	}

	cmd_boot(0, 0);
}

APP_START(gta4xl_boot)
	.entry = gta4xl_boot_task,
	.flags = 0,
APP_END
