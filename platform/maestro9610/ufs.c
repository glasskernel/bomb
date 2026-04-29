/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * This software is proprietary of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied or distributed, transmitted,
 * transcribed, stored in a retrieval system or translated into any human or computer language in any form by any means,
 * electronic, mechanical, manual or otherwise, or disclosed
 * to third parties without the express written permission of Samsung Electronics.
 */

#include <lk/reg.h>
#include <dev/ufs.h>
#include <platform/exynos9610.h>

struct ufs_host;

#define UFS_SCLK					133250000
#define CNT_VAL_1US_MASK				0x3ff
#define UFSHCI_VS_1US_TO_CNT_VAL			0x110C
#define UFSHCI_VS_UFSHCI_V2P1_CTRL			0x118C
#define IA_TICK_SEL					(1 << 16)

#define EXYNOS9610_TOP_BASE				0x12100000
#define EXYNOS9610_SYSREG_FSYS_BASE			0x13410000

#define CLK_CON_MUX_CLKCMU_FSYS_UFS_EMBD		(EXYNOS9610_TOP_BASE + 0x1048)
#define CLK_CON_DIV_CLKCMU_FSYS_UFS_EMBD		(EXYNOS9610_TOP_BASE + 0x1850)
#define UFS_DMA_COHERENCY_CTRL				(EXYNOS9610_SYSREG_FSYS_BASE + 0x1010)

#define UFS_GPIO_CON					(EXYNOS9610_GPIO_TOP_BASE + 0x140)
#define UFS_GPIO_DAT					(EXYNOS9610_GPIO_TOP_BASE + 0x144)
#define UFS_GPIO_PUD					(EXYNOS9610_GPIO_TOP_BASE + 0x148)

#define UFS_CLKCMU_TIMEOUT				100

static void ufs_vs_set_1us_to_cnt(struct ufs_host *ufs)
{
	u32 reg;

	reg = readl(ufs->ioaddr + UFSHCI_VS_UFSHCI_V2P1_CTRL);
	reg |= IA_TICK_SEL;
	writel(reg, ufs->ioaddr + UFSHCI_VS_UFSHCI_V2P1_CTRL);

	writel((UFS_SCLK / 1000000) & CNT_VAL_1US_MASK,
	       ufs->ioaddr + UFSHCI_VS_1US_TO_CNT_VAL);
}

static void ufs_set_unipro_clk(struct ufs_host *ufs)
{
	int timeout = 0;

	writel(2, CLK_CON_DIV_CLKCMU_FSYS_UFS_EMBD);
	do {
		timeout++;
	} while ((readl(CLK_CON_DIV_CLKCMU_FSYS_UFS_EMBD) & 0x10000) &&
		 timeout < UFS_CLKCMU_TIMEOUT);
	if (timeout == UFS_CLKCMU_TIMEOUT)
		printf("ERROR(UFS): divider setup timed out\n");

	timeout = 0;
	writel(1, CLK_CON_MUX_CLKCMU_FSYS_UFS_EMBD);
	do {
		timeout++;
	} while ((readl(CLK_CON_MUX_CLKCMU_FSYS_UFS_EMBD) & 0x10000) &&
		 timeout < UFS_CLKCMU_TIMEOUT);
	if (timeout == UFS_CLKCMU_TIMEOUT)
		printf("ERROR(UFS): mux setup timed out\n");

	ufs_vs_set_1us_to_cnt(ufs);
}

int ufs_board_init(int host_index, struct ufs_host *ufs)
{
	u32 reg;
	//u32 err;

	if (host_index) {
		printf("Currently multi UFS host is not supported!\n");
		return -1;
	}

	/* mmio */
	sprintf(ufs->host_name,"ufs%d", host_index);
	ufs->irq = 157;
	ufs->ioaddr = (void __iomem *)0x13520000;
	ufs->vs_addr = (void __iomem *)(0x13520000 + 0x1100);
	ufs->fmp_addr = (void __iomem *)0x13530000;
	ufs->unipro_addr = (void __iomem *)(0x13520000 - 0x10000);
	ufs->phy_pma = (void __iomem *)(0x13520000 + 0x4000);

	/* power source changed compared with before */
	ufs->dev_pwr_addr = (void __iomem *)(0x139B0000 + 0x144);
	ufs->dev_pwr_shift = 0;

	ufs->phy_iso_addr = (void __iomem *)(0x11860000 + 0x724);

	ufs->host_index = host_index;

	ufs->mclk_rate = UFS_SCLK;
	ufs_set_unipro_clk(ufs);
	ufs->gear_mode = 3;

	// TODO:
	//set_ufs_clk(host_index);
	//err = exynos_pinmux_config(PERIPH_ID_UFS0, PINMUX_FLAG_NONE);

	/* GPIO configurations, rst_n and refclk */
	reg = *(volatile u32 *)0x13490008;
	reg &= ~(0xFF);
	*(volatile u32 *)0x13490008 = reg;

	reg = *(volatile u32 *)0x13490000;
	reg &= ~(0xFF);
	reg |= 0x33;
	*(volatile u32 *)0x13490000 = reg;

	/* UFS fixed regulator: gpg4-0 output high. */
	reg = readl(UFS_GPIO_PUD);
	reg &= ~0x3;
	writel(reg, UFS_GPIO_PUD);

	reg = readl(UFS_GPIO_CON);
	reg &= ~0xF;
	reg |= 0x1;
	writel(reg, UFS_GPIO_CON);

	reg = readl(UFS_DMA_COHERENCY_CTRL);
	reg |= (1 << 8) | (1 << 9);
	writel(reg, UFS_DMA_COHERENCY_CTRL);

	return 0;
}

void ufs_pre_vendor_setup(struct ufs_host *ufs)
{

}
