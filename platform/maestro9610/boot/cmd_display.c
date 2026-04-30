/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * This software is proprietary of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied or distributed, transmitted,
 * transcribed, stored in a retrieval system or translated into any human or computer language in any form by any means,
 * electronic, mechanical, manual or otherwise, or disclosed
 * to third parties without the express written permission of Samsung Electronics.
 */

#include <stdio.h>
#include <libfdt.h>
#include <platform/fdt.h>

extern u32 ddi_id;

static int find_dsim_node(void)
{
	const char *path;
	int noff;

	path = fdt_get_alias(fdt_dtb, "dsim0");
	if (path) {
		noff = fdt_path_offset(fdt_dtb, path);
		if (noff >= 0)
			return noff;
	}

	noff = fdt_path_offset(fdt_dtb, "/dsim");
	if (noff >= 0)
		return noff;

	noff = fdt_path_offset(fdt_dtb, "/dsim@0x148E0000");
	if (noff >= 0)
		return noff;

	return fdt_node_offset_by_compatible(fdt_dtb, -1, "samsung,exynos9-dsim");
}

void configure_ddi_id(void)
{
	int noff;

	printf("Attached DDI id is [%#x]\n", ddi_id);

	if (!ddi_id)
		return;

	noff = find_dsim_node();
	if (noff < 0) {
		printf("DSIM node not found for DDI id update: %s\n",
		       fdt_strerror(noff));
		return;
	}

	fdt_setprop_u32(fdt_dtb, noff, "ddi_id", ddi_id);
#if 0
	/* the path(node) name, /dsim is same with /dsim@0x148E0000 */
	get_fdt_val("/dsim", "ddi_id", (char *)str);
	printf("ddi_id (%s)\n", str);
#endif
}
