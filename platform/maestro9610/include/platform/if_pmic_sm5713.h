/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * This software is proprietary of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied or distributed, transmitted,
 * transcribed, stored in a retrieval system or translated into any human or computer language in any form by any means,
 * electronic, mechanical, manual or otherwise, or disclosed
 * to third parties without the express written permission of Samsung Electronics.
 */

#ifndef __IF_PMIC_SM5713_H__
#define __IF_PMIC_SM5713_H__

#define SM5713_MUIC_W_ADDR		0x4A
#define SM5713_MUIC_R_ADDR		0x4B

#define SM5713_MUIC_REG_INT1		0x01
#define SM5713_MUIC_REG_INT2		0x02
#define SM5713_MUIC_REG_CONTROL		0x05
#define SM5713_MUIC_REG_MANUAL_SW	0x06
#define SM5713_MUIC_REG_DEV_TYPE1	0x07
#define SM5713_MUIC_REG_DEV_TYPE2	0x08
#define SM5713_MUIC_REG_DP_RESET	0x09
#define SM5713_MUIC_REG_VBUS		0x3E
#define SM5713_MUIC_REG_ADC		0x51
#define SM5713_MUIC_REG_CFG1		0x68

void IIC_SM5713_ESetport(void);
void IIC_SM5713_ERead(unsigned char ChipId,
		unsigned char IicAddr, unsigned char *IicData);
void IIC_SM5713_EWrite(unsigned char ChipId,
		unsigned char IicAddr, unsigned char IicData);
void sm5713_muic_init(void);
void muic_sw_usb(void);
void muic_sw_uart(void);
int muic_get_vbus(void);

#endif /* __IF_PMIC_SM5713_H__ */
