/*
 * Generic DWC3 fastboot glue for Exynos 9610/9611.
 */

#include <lk/err.h>
#include <lk/init.h>
#include <lk/list.h>
#include <lk/reg.h>
#include <dev/rpmb.h>
#include <dev/scsi.h>
#include <dev/usb/dwc3-config.h>
#include <dev/usb/fastboot.h>
#include <dev/usb/gadget.h>
#include <dev/usb/phy-samsung-usb-cal.h>
#include <lk/debug.h>
#include <platform/chip_id.h>
#include <platform/sfr.h>
#include <string.h>

/* Forward declaration */
void phy_usb_exynos_system_init(int num_phy_port, bool en);
void phy_exynos_usb_v3p1_enable_dp_pullup(struct exynos_usbphy_info *info);
void phy_exynos_usb_v3p1_disable_dp_pullup(struct exynos_usbphy_info *info);

static const char vendor_str[] = "Samsung - " PLATFORM;
static const char product_str[] = TARGET " - lk3rd";
static char serial_id[17] = "No Serial";

static unsigned int dwc3_isr_num = EXYNOS9610_USB_INT_NUM + 32;

/*
 * USB Controller power registers (relative to USB_LINK_BASE 0x13200000)
 * Based on stock S-BOOT reverse engineering
 */
#define USB_REG_PWR1		0xC200
#define USB_REG_PWR2		0xC2C0
#define USB_PWR_BIT		(1 << 31)
#define USB_REG_GSBUSCFG0	0xC100
#define USB_REG_GSBUSCFG1	0xC104
#define USB_REG_GCTL		0xC110
#define USB_REG_GUSB2PHYCFG	0xC200
#define USB_REG_GUSB3PIPECTL	0xC2C0
#define USB_REG_USB2PHYCFG_MASK	0xFFFFC000
#define USB_REG_USB2PHYCFG_KEEP	0x000002BF
#define USB_REG_USB2PHYCFG_BL	0x00002400
#define USB_REG_GUSB3_SUSPEND	(1 << 17)
#define USB_REG_GCTL_KEEP_HIGH	0x0007C000
#define USB_REG_GCTL_KEEP_LOW	0x00000F3F
#define USB_REG_GCTL_DEVICE	(2 << 12)
#define USB_REG_GCTL_U2RST_ECN	(1 << 16)
#define USB_REG_GCTL_MASTER_FILT_BYPASS	(1 << 18)
#define USB_PHY_PMU_ENABLE	0x3

#define USB_PHY_REG_LINK_CTRL	0x04
#define USB_PHY_REG_LINK_PORT	0x08
#define USB_PHY_REG_LINK_DEBUG_L	0x0c
#define USB_PHY_REG_LINK_DEBUG_H	0x10
#define USB_PHY_REG_CLKRST	0x20
#define USB_PHY_REG_PWR	0x24
#define USB_PHY_REG_UTMI	0x50
#define USB_PHY_REG_HSP	0x54
#define USB_PHY_REG_HSP_TUNE	0x58
#define USB_PHY_REG_HSP_TEST	0x5c
#define USB_PHY_LINK_BUS_FILTER_BYPASS	(0xf << 4)
#define USB_PHY_UTMI_FORCE_VBUSVALID	(1 << 5)
#define USB_PHY_UTMI_FORCE_BVALID	(1 << 4)
#define USB_PHY_UTMI_DP_PULLDOWN	(1 << 3)
#define USB_PHY_UTMI_DM_PULLDOWN	(1 << 2)
#define USB_PHY_UTMI_FORCE_SUSPEND	(1 << 1)
#define USB_PHY_UTMI_FORCE_SLEEP	(1 << 0)
#define USB_PHY_HSP_VBUSVLDEXTSEL	(1 << 13)
#define USB_PHY_HSP_VBUSVLDEXT	(1 << 12)

int gadget_get_vendor_string(void)
{
	return get_str_id(vendor_str, strlen(vendor_str));
}

int gadget_get_product_string(void)
{
	return get_str_id(product_str, strlen(product_str));
}

static void reverse_serialno_string(void)
{
	char tmp[17];
	int i;

	memcpy(tmp, serial_id, sizeof(serial_id));

	for (i = 0; i < 16; i++)
		serial_id[i] = tmp[15 - i];
}

static const char *make_serial_string(void)
{
	u8 i, j;
	int chip_id[2];

	if (strcmp(serial_id, "No Serial"))
		return serial_id;

	chip_id[0] = readl(EXYNOS9610_PRO_ID + CHIPID0_OFFSET);
	chip_id[1] = readl(EXYNOS9610_PRO_ID + CHIPID1_OFFSET) & 0xFFFF;

	for (j = 0; j < 2; j++) {
		u32 hex = chip_id[j];
		char *str = &serial_id[j * 8];

		for (i = 0; i < 8; i++) {
			if ((hex & 0xF) > 9)
				*str++ = 'a' + (hex & 0xF) - 10;
			else
				*str++ = '0' + (hex & 0xF);
			hex >>= 4;
		}
	}

	reverse_serialno_string();

	return serial_id;
}

int gadget_get_serial_string(void)
{
	return get_str_id(make_serial_string(), 16);
}

const char *fastboot_get_product_string(void)
{
	return product_str;
}

const char *fastboot_get_serialno_string(void)
{
	return make_serial_string();
}

static void sboot_usb_link_config(void)
{
	u32 reg;

	reg = readl((void *)(EXYNOS9610_USB_LINK_BASE + USB_REG_GUSB2PHYCFG));
	reg = (reg & USB_REG_USB2PHYCFG_MASK) |
	      (reg & USB_REG_USB2PHYCFG_KEEP) |
	      USB_REG_USB2PHYCFG_BL;
	writel(reg, (void *)(EXYNOS9610_USB_LINK_BASE + USB_REG_GUSB2PHYCFG));

	reg = readl((void *)(EXYNOS9610_USB_LINK_BASE + USB_REG_GUSB3PIPECTL));
	reg &= ~USB_REG_GUSB3_SUSPEND;
	writel(reg, (void *)(EXYNOS9610_USB_LINK_BASE + USB_REG_GUSB3PIPECTL));

	reg = readl((void *)(EXYNOS9610_USB_LINK_BASE + USB_REG_GCTL));
	reg = (reg & USB_REG_GCTL_KEEP_HIGH) |
	      (reg & USB_REG_GCTL_KEEP_LOW) |
	      USB_REG_GCTL_DEVICE |
	      USB_REG_GCTL_U2RST_ECN |
	      USB_REG_GCTL_MASTER_FILT_BYPASS;
	writel(reg, (void *)(EXYNOS9610_USB_LINK_BASE + USB_REG_GCTL));

	writel(0x2222000f,
	       (void *)(EXYNOS9610_USB_LINK_BASE + USB_REG_GSBUSCFG0));
	writel(0x00000f00,
	       (void *)(EXYNOS9610_USB_LINK_BASE + USB_REG_GSBUSCFG1));
}

int dwc3_plat_init(struct dwc3_plat_config *plat_config)
{
	plat_config->base = (void *)EXYNOS9610_USB_LINK_BASE;
	plat_config->num_hs_phy = 1;
	plat_config->array_intr = &dwc3_isr_num;
	plat_config->num_intr = 1;
	plat_config->config.m_eBurstLength = 0xf;
	strcpy(plat_config->ssphy_type, "snps_gen1");

	/*
	 * Power on USB Link Controller.
	 * Based on stock S-BOOT exynos_usb_init_core() sequence.
	 * This must be done before DWC3 device init.
	 */
	u32 reg;

	reg = readl((void *)(EXYNOS9610_USB_LINK_BASE + USB_REG_PWR1));
	reg |= USB_PWR_BIT;
	writel(reg, (void *)(EXYNOS9610_USB_LINK_BASE + USB_REG_PWR1));

	reg = readl((void *)(EXYNOS9610_USB_LINK_BASE + USB_REG_PWR2));
	reg |= USB_PWR_BIT;
	writel(reg, (void *)(EXYNOS9610_USB_LINK_BASE + USB_REG_PWR2));

	/* Call PHY/system init */
	phy_usb_exynos_system_init(0, true);

	/* Power off - stock bootloader does this too before reconfiguring */
	reg = readl((void *)(EXYNOS9610_USB_LINK_BASE + USB_REG_PWR1));
	reg &= ~USB_PWR_BIT;
	writel(reg, (void *)(EXYNOS9610_USB_LINK_BASE + USB_REG_PWR1));

	reg = readl((void *)(EXYNOS9610_USB_LINK_BASE + USB_REG_PWR2));
	reg &= ~USB_PWR_BIT;
	writel(reg, (void *)(EXYNOS9610_USB_LINK_BASE + USB_REG_PWR2));

	sboot_usb_link_config();

	return 0;
}

static struct dwc3_dev_config dwc3_dev_config = {
	.speed = "high",
	.m_uEventBufDepth = 64,
	.m_uCtrlBufSize = 128,
	.m_ucU1ExitValue = 10,
	.m_usU2ExitValue = 257,
	.need_cache_ops = 1,
};

int dwc3_dev_plat_init(void **base_addr, struct dwc3_dev_config **plat_config)
{
	*base_addr = (void *)EXYNOS9610_USB_LINK_BASE;
	*plat_config = &dwc3_dev_config;

	return 0;
}

static struct exynos_usb_tune_param usbcal_20phy_tune[] = {
	{ .name = "tx_pre_emp", .value = 0x3, },
	{ .name = "tx_pre_emp_plus", .value = 0x0, },
	{ .name = "tx_vref", .value = 0xf, },
	{ .name = "rx_sqrx", .value = 0x7, },
	{ .name = "tx_rise", .value = 0x3, },
	{ .name = "compdis", .value = 0x7, },
	{ .name = "tx_hsxv", .value = 0x3, },
	{ .name = "tx_fsls", .value = 0x3, },
	{ .name = "tx_res", .value = 0x3, },
	{ .name = "utim_clk", .value = USBPHY_UTMI_PHYCLOCK, },
	{ .value = EXYNOS_USB_TUNE_LAST, },
};

static struct exynos_usbphy_info usbphy_cal_info = {
	.version = EXYNOS_USBCON_VER_03_0_0,
	.refclk = USBPHY_REFCLK_DIFF_26MHZ,
	.refsel = USBPHY_REFSEL_CLKCORE,
	.not_used_vbus_pad = true,
	.use_io_for_ovc = 0,
	.common_block_disable = true,
	.regs_base = (void *)EXYNOS9610_USB_PHY_BASE,
	.tune_param = usbcal_20phy_tune,
	.used_phy_port = 0,
	.hs_rewa = 1,
};

static void maestro9610_usb_phy_force_attach(void)
{
	void *base = usbphy_cal_info.regs_base;
	u32 reg;

	reg = readl(base + USB_PHY_REG_LINK_CTRL);
	reg |= USB_PHY_LINK_BUS_FILTER_BYPASS;
	writel(reg, base + USB_PHY_REG_LINK_CTRL);

	reg = readl(base + USB_PHY_REG_UTMI);
	reg |= USB_PHY_UTMI_FORCE_BVALID | USB_PHY_UTMI_FORCE_VBUSVALID;
	reg &= ~(USB_PHY_UTMI_FORCE_SUSPEND |
		 USB_PHY_UTMI_FORCE_SLEEP |
		 USB_PHY_UTMI_DP_PULLDOWN |
		 USB_PHY_UTMI_DM_PULLDOWN);
	writel(reg, base + USB_PHY_REG_UTMI);

	reg = readl(base + USB_PHY_REG_HSP);
	reg |= USB_PHY_HSP_VBUSVLDEXTSEL | USB_PHY_HSP_VBUSVLDEXT;
	writel(reg, base + USB_PHY_REG_HSP);
}

static void maestro9610_usb_phy_dump_state(void)
{
	void *base = usbphy_cal_info.regs_base;

	printf("[phy_diag] LINK_CTRL=0x%08x LINK_PORT=0x%08x DEBUG=%08x/%08x\n",
	       readl(base + USB_PHY_REG_LINK_CTRL),
	       readl(base + USB_PHY_REG_LINK_PORT),
	       readl(base + USB_PHY_REG_LINK_DEBUG_H),
	       readl(base + USB_PHY_REG_LINK_DEBUG_L));
	printf("[phy_diag] CLKRST=0x%08x PWR=0x%08x UTMI=0x%08x HSP=0x%08x\n",
	       readl(base + USB_PHY_REG_CLKRST),
	       readl(base + USB_PHY_REG_PWR),
	       readl(base + USB_PHY_REG_UTMI),
	       readl(base + USB_PHY_REG_HSP));
	printf("[phy_diag] HSP_TUNE=0x%08x HSP_TEST=0x%08x\n",
	       readl(base + USB_PHY_REG_HSP_TUNE),
	       readl(base + USB_PHY_REG_HSP_TEST));
}

void dwc3_plat_phy_dp_pullup(bool en_pullup)
{
	if (en_pullup) {
		maestro9610_usb_phy_force_attach();
		phy_exynos_usb_v3p1_enable_dp_pullup(&usbphy_cal_info);
	} else {
		phy_exynos_usb_v3p1_disable_dp_pullup(&usbphy_cal_info);
	}

	printf("[phy_diag] dp_pullup=%d\n", en_pullup);
	maestro9610_usb_phy_dump_state();
}

void dwc3_dev_plat_dump_state(void)
{
	maestro9610_usb_phy_dump_state();
}

static void register_phy_cal_infor(uint level)
{
	phy_usb_exynos_register_cal_infor(&usbphy_cal_info);
}
LK_INIT_HOOK(register_phy_cal_infor, &register_phy_cal_infor,
	     LK_INIT_LEVEL_KERNEL);

void phy_usb_exynos_system_init(int num_phy_port, bool en)
{
	writel(en ? USB_PHY_PMU_ENABLE : 0,
	       EXYNOS9610_POWER_BASE + EXYNOS9610_USB_PHY_CONTROL_OFFSET);

	/*
	 * Additional USB link power control - for USBDP combo PHY control
	 * The EXYNOS9610_USB_PHY_CONTROL_OFFSET at 0x704 handles PMU-based isolation.
	 */
}

void exynos_usb_cci_control(int on_off)
{
}

void platform_prepare_reboot(void)
{
	scsi_do_ssu();
}

void platform_do_reboot(const char *cmd_buf)
{
	if (!memcmp(cmd_buf, "reboot-bootloader", strlen("reboot-bootloader")) ||
	    !memcmp(cmd_buf, "reboot-fastboot", strlen("reboot-fastboot")))
		writel(REBOOT_MODE_FASTBOOT_USER, EXYNOS_POWER_SYSIP_DAT0);
	else
		writel(0, EXYNOS_POWER_SYSIP_DAT0);

	writel(0, CONFIG_RAMDUMP_SCRATCH);
	writel(0, EXYNOS_POWER_RST_STAT);
	writel(1, EXYNOS9610_SWRESET);
}
