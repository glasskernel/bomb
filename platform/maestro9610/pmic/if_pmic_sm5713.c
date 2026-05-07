/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * This software is proprietary of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied or distributed, transmitted,
 * transcribed, stored in a retrieval system or translated into any human or computer language in any form by any means,
 * electronic, mechanical, manual or otherwise, or disclosed
 * to third parties without the express written permission of Samsung Electronics.
 */

#include <lk/debug.h>
#include <platform/delay.h>
#include <platform/if_pmic_sm5713.h>
#include <stdio.h>
#include <sys/types.h>

#define GPP0BASE	(0x139b0000)
#define GPP0CON		*(volatile unsigned int *)(GPP0BASE + 0x0)
#define GPP0DAT		*(volatile unsigned int *)(GPP0BASE + 0x4)
#define GPP0PUD		*(volatile unsigned int *)(GPP0BASE + 0x8)

/* SDA: GPP0_2, SCL: GPP0_3 */
#define GPIO_DAT_SM5713	GPP0DAT
#define GPIO_DAT_SHIFT		(2)
#define GPIO_PUD_SM5713	GPP0PUD &= ~(0xff << (GPIO_DAT_SHIFT * 4))

#define IIC_SM5713_ESCL_Hi	GPP0DAT |= (0x1 << (GPIO_DAT_SHIFT + 1))
#define IIC_SM5713_ESCL_Lo	GPP0DAT &= ~(0x1 << (GPIO_DAT_SHIFT + 1))
#define IIC_SM5713_ESDA_Hi	GPP0DAT |= (0x1 << GPIO_DAT_SHIFT)
#define IIC_SM5713_ESDA_Lo	GPP0DAT &= ~(0x1 << GPIO_DAT_SHIFT)

#define IIC_SM5713_ESCL_INP	GPP0CON &= ~(0xf << ((GPIO_DAT_SHIFT + 1) * 4))
#define IIC_SM5713_ESCL_OUTP	GPP0CON = (GPP0CON & ~(0xf << ((GPIO_DAT_SHIFT + 1) * 4))) \
					| (0x1 << ((GPIO_DAT_SHIFT + 1) * 4))
#define IIC_SM5713_ESDA_INP	GPP0CON &= ~(0xf << (GPIO_DAT_SHIFT * 4))
#define IIC_SM5713_ESDA_OUTP	GPP0CON = (GPP0CON & ~(0xf << (GPIO_DAT_SHIFT * 4))) \
					| (0x1 << (GPIO_DAT_SHIFT * 4))

#define DELAY		100

static void Delay(void)
{
	unsigned long i = 0;

	for (i = 0; i < DELAY; i++)
		;
}

static void IIC_SM5713_SCLH_SDAH(void)
{
	IIC_SM5713_ESCL_Hi;
	IIC_SM5713_ESDA_Hi;
	Delay();
}

static void IIC_SM5713_SCLH_SDAL(void)
{
	IIC_SM5713_ESCL_Hi;
	IIC_SM5713_ESDA_Lo;
	Delay();
}

static void IIC_SM5713_SCLL_SDAH(void)
{
	IIC_SM5713_ESCL_Lo;
	IIC_SM5713_ESDA_Hi;
	Delay();
}

static void IIC_SM5713_SCLL_SDAL(void)
{
	IIC_SM5713_ESCL_Lo;
	IIC_SM5713_ESDA_Lo;
	Delay();
}

static void IIC_SM5713_ELow(void)
{
	IIC_SM5713_SCLL_SDAL();
	IIC_SM5713_SCLH_SDAL();
	IIC_SM5713_SCLH_SDAL();
	IIC_SM5713_SCLL_SDAL();
}

static void IIC_SM5713_EHigh(void)
{
	IIC_SM5713_SCLL_SDAH();
	IIC_SM5713_SCLH_SDAH();
	IIC_SM5713_SCLH_SDAH();
	IIC_SM5713_SCLL_SDAH();
}

static void IIC_SM5713_EStart(void)
{
	IIC_SM5713_SCLH_SDAH();
	IIC_SM5713_SCLH_SDAL();
	Delay();
	IIC_SM5713_SCLL_SDAL();
}

static void IIC_SM5713_EEnd(void)
{
	IIC_SM5713_SCLL_SDAL();
	IIC_SM5713_SCLH_SDAL();
	Delay();
	IIC_SM5713_SCLH_SDAH();
}

static void IIC_SM5713_EAck_write(void)
{
	unsigned long ack = 0;

	IIC_SM5713_ESDA_INP;

	IIC_SM5713_ESCL_Lo;
	Delay();
	IIC_SM5713_ESCL_Hi;
	Delay();
	ack = GPIO_DAT_SM5713;
	IIC_SM5713_ESCL_Hi;
	Delay();
	IIC_SM5713_ESCL_Hi;
	Delay();

	IIC_SM5713_ESDA_OUTP;

	ack = (ack >> GPIO_DAT_SHIFT) & 0x1;
	(void)ack;

	IIC_SM5713_SCLL_SDAL();
}

static void IIC_SM5713_EAck_read(void)
{
	IIC_SM5713_ESDA_OUTP;

	IIC_SM5713_ESCL_Lo;
	IIC_SM5713_ESCL_Lo;
	IIC_SM5713_ESDA_Hi;
	IIC_SM5713_ESCL_Hi;
	IIC_SM5713_ESCL_Hi;
	IIC_SM5713_ESDA_INP;

	IIC_SM5713_SCLL_SDAL();
}

void IIC_SM5713_ESetport(void)
{
	GPIO_PUD_SM5713;

	IIC_SM5713_ESCL_Hi;
	IIC_SM5713_ESDA_Hi;

	IIC_SM5713_ESCL_OUTP;
	IIC_SM5713_ESDA_OUTP;

	Delay();
}

void IIC_SM5713_EWrite(unsigned char ChipId,
		unsigned char IicAddr, unsigned char IicData)
{
	unsigned long i = 0;

	IIC_SM5713_EStart();

	for (i = 7; i > 0; i--) {
		if ((ChipId >> i) & 0x0001)
			IIC_SM5713_EHigh();
		else
			IIC_SM5713_ELow();
	}

	IIC_SM5713_ELow();
	IIC_SM5713_EAck_write();

	for (i = 8; i > 0; i--) {
		if ((IicAddr >> (i - 1)) & 0x0001)
			IIC_SM5713_EHigh();
		else
			IIC_SM5713_ELow();
	}

	IIC_SM5713_EAck_write();

	for (i = 8; i > 0; i--) {
		if ((IicData >> (i - 1)) & 0x0001)
			IIC_SM5713_EHigh();
		else
			IIC_SM5713_ELow();
	}

	IIC_SM5713_EAck_write();

	IIC_SM5713_EEnd();
}

void IIC_SM5713_ERead(unsigned char ChipId,
		unsigned char IicAddr, unsigned char *IicData)
{
	unsigned long i = 0;
	unsigned long reg = 0;
	unsigned char data = 0;

	IIC_SM5713_EStart();

	for (i = 7; i > 0; i--) {
		if ((ChipId >> i) & 0x0001)
			IIC_SM5713_EHigh();
		else
			IIC_SM5713_ELow();
	}

	IIC_SM5713_ELow();
	IIC_SM5713_EAck_write();

	for (i = 8; i > 0; i--) {
		if ((IicAddr >> (i - 1)) & 0x0001)
			IIC_SM5713_EHigh();
		else
			IIC_SM5713_ELow();
	}

	IIC_SM5713_EAck_write();

	IIC_SM5713_EStart();

	for (i = 7; i > 0; i--) {
		if ((ChipId >> i) & 0x0001)
			IIC_SM5713_EHigh();
		else
			IIC_SM5713_ELow();
	}

	IIC_SM5713_EHigh();
	IIC_SM5713_EAck_write();

	IIC_SM5713_ESDA_INP;

	IIC_SM5713_ESCL_Lo;
	IIC_SM5713_ESCL_Lo;
	Delay();

	for (i = 8; i > 0; i--) {
		IIC_SM5713_ESCL_Lo;
		IIC_SM5713_ESCL_Lo;
		Delay();
		IIC_SM5713_ESCL_Hi;
		IIC_SM5713_ESCL_Hi;
		Delay();
		reg = GPIO_DAT_SM5713;
		IIC_SM5713_ESCL_Hi;
		IIC_SM5713_ESCL_Hi;
		Delay();
		IIC_SM5713_ESCL_Lo;
		IIC_SM5713_ESCL_Lo;
		Delay();

		reg = (reg >> GPIO_DAT_SHIFT) & 0x1;

		data |= reg << (i - 1);
	}

	IIC_SM5713_EAck_read();
	IIC_SM5713_ESDA_OUTP;

	IIC_SM5713_EEnd();

	*IicData = data;
}

void sm5713_muic_init(void)
{
	unsigned char reg = 0;
	unsigned char int1 = 0;
	unsigned char int2 = 0;
	unsigned char dev1 = 0;
	unsigned char dev2 = 0;

	IIC_SM5713_ESetport();

	IIC_SM5713_EWrite(SM5713_MUIC_W_ADDR, SM5713_MUIC_REG_CONTROL, 0x24);
	IIC_SM5713_ERead(SM5713_MUIC_R_ADDR, SM5713_MUIC_REG_CONTROL, &reg);
	printf("[MUIC] SM5713 CONTROL: 0x%02x\n", reg);

	IIC_SM5713_ERead(SM5713_MUIC_R_ADDR, SM5713_MUIC_REG_CFG1, &reg);
	reg &= 0xdb;
	IIC_SM5713_EWrite(SM5713_MUIC_W_ADDR, SM5713_MUIC_REG_CFG1, reg);
	IIC_SM5713_ERead(SM5713_MUIC_R_ADDR, SM5713_MUIC_REG_CFG1, &reg);
	printf("[MUIC] SM5713 CFG1 init: 0x%02x\n", reg);

	IIC_SM5713_ERead(SM5713_MUIC_R_ADDR, SM5713_MUIC_REG_INT1, &int1);
	IIC_SM5713_ERead(SM5713_MUIC_R_ADDR, SM5713_MUIC_REG_INT2, &int2);
	IIC_SM5713_ERead(SM5713_MUIC_R_ADDR, SM5713_MUIC_REG_DEV_TYPE1, &dev1);
	IIC_SM5713_ERead(SM5713_MUIC_R_ADDR, SM5713_MUIC_REG_DEV_TYPE2, &dev2);
	printf("[MUIC] SM5713 INT1:0x%02x INT2:0x%02x DEV1:0x%02x DEV2:0x%02x\n",
	       int1, int2, dev1, dev2);

	if ((dev1 & 0x14) || (dev2 & 0x60)) {
		IIC_SM5713_EWrite(SM5713_MUIC_W_ADDR, SM5713_MUIC_REG_DP_RESET, 0x20);
		mdelay(20);
		IIC_SM5713_EWrite(SM5713_MUIC_W_ADDR, SM5713_MUIC_REG_DP_RESET, 0x00);
		printf("[MUIC] SM5713 DP reset\n");
	}
}

void muic_sw_usb(void)
{
	unsigned char reg = 0;

	IIC_SM5713_ESetport();

	IIC_SM5713_ERead(SM5713_MUIC_R_ADDR, SM5713_MUIC_REG_CFG1, &reg);
	printf("[MUIC] switch_to_usb_vbus CFG1:0x%02x -> 0x%02x\n",
	       reg, reg | 0x04);
	reg |= 0x04;
	IIC_SM5713_EWrite(SM5713_MUIC_W_ADDR, SM5713_MUIC_REG_CFG1, reg);
	IIC_SM5713_EWrite(SM5713_MUIC_W_ADDR, SM5713_MUIC_REG_MANUAL_SW, 0x09);
	IIC_SM5713_ERead(SM5713_MUIC_R_ADDR, SM5713_MUIC_REG_MANUAL_SW, &reg);
	printf("[MUIC] switch_to_usb_vbus MANUAL_SW:0x%02x\n", reg);
}

void muic_sw_uart(void)
{
	unsigned char reg = 0;

	IIC_SM5713_ESetport();

	IIC_SM5713_EWrite(SM5713_MUIC_W_ADDR, SM5713_MUIC_REG_MANUAL_SW, 0x1b);
	IIC_SM5713_ERead(SM5713_MUIC_R_ADDR, SM5713_MUIC_REG_CFG1, &reg);
	reg |= 0x04;
	IIC_SM5713_EWrite(SM5713_MUIC_W_ADDR, SM5713_MUIC_REG_CFG1, reg);
}

int muic_get_vbus(void)
{
	unsigned char vbvolt = 0;

	IIC_SM5713_ESetport();
	IIC_SM5713_ERead(SM5713_MUIC_R_ADDR, SM5713_MUIC_REG_VBUS, &vbvolt);
	printf("%s: vbvolt: 0x%02x\n", __func__, vbvolt & 0x1);

	return vbvolt & 0x1;
}
