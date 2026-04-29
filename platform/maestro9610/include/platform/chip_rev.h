#ifndef __CHIP_REV_H__
#define __CHIP_REV_H__

struct chip_rev_info {
	unsigned int main;
	unsigned int sub;
};

extern struct chip_rev_info s5p_chip_rev;

#endif /* __CHIP_REV_H__ */
