/*
 * Copyright © 2020 Valve Corporation
 *
 * based on amdgpu winsys.
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#include "radv_null_winsys_public.h"

#include "radv_null_bo.h"
#include "radv_null_cs.h"

#include "ac_llvm_util.h"

/* Hardcode some PCI IDs to allow external tools to recognize chips. */
static const struct {
	enum radeon_family family;
	uint32_t pci_id;
} pci_ids[] = {
	{ CHIP_TAHITI, 0x6780 },
	{ CHIP_PITCAIRN, 0x6800 },
	{ CHIP_VERDE, 0x6820 },
	{ CHIP_OLAND, 0x6060 },
	{ CHIP_HAINAN, 0x6660 },
	{ CHIP_BONAIRE, 0x6640 },
	{ CHIP_KAVERI, 0x1304 },
	{ CHIP_KABINI, 0x9830 },
	{ CHIP_HAWAII, 0x67A0 },
	{ CHIP_TONGA, 0x6920 },
	{ CHIP_ICELAND, 0x6900 },
	{ CHIP_CARRIZO, 0x9870 },
	{ CHIP_FIJI, 0x7300 },
	{ CHIP_STONEY, 0x98E4 },
	{ CHIP_POLARIS10, 0x67C0 },
	{ CHIP_POLARIS11, 0x67E0 },
	{ CHIP_POLARIS12, 0x6980 },
	{ CHIP_VEGAM, 0x694C },
	{ CHIP_VEGA12, 0x6860 },
	{ CHIP_VEGA12, 0x69A0 },
	{ CHIP_VEGA20, 0x66A0 },
	{ CHIP_RAVEN, 0x15DD },
	{ CHIP_RENOIR, 0x1636 },
	{ CHIP_ARCTURUS, 0x738C },
	{ CHIP_NAVI10, 0x7310 },
	{ CHIP_NAVI12, 0x7360 },
	{ CHIP_NAVI14, 0x7340 },
};

static uint32_t
radv_null_winsys_get_pci_id(enum radeon_family family)
{
	for (unsigned i = 0; i < ARRAY_SIZE(pci_ids); i++) {
		if (pci_ids[i].family == family)
			return pci_ids[i].pci_id;
	}
	return 0;
}

static void radv_null_winsys_query_info(struct radeon_winsys *rws,
					struct radeon_info *info)
{
	const char *family = getenv("RADV_FORCE_FAMILY");
	unsigned i;

	info->chip_class = CLASS_UNKNOWN;
	info->family = CHIP_UNKNOWN;

	for (i = CHIP_TAHITI; i < CHIP_LAST; i++) {
		if (!strcmp(family, ac_get_llvm_processor_name(i))) {
			/* Override family and chip_class. */
			info->family = i;
			info->name = "OVERRIDDEN";

			if (i >= CHIP_NAVI10)
				info->chip_class = GFX10;
			else if (i >= CHIP_VEGA10)
				info->chip_class = GFX9;
			else if (i >= CHIP_TONGA)
				info->chip_class = GFX8;
			else if (i >= CHIP_BONAIRE)
				info->chip_class = GFX7;
			else
				info->chip_class = GFX6;
		}
	}

	if (info->family == CHIP_UNKNOWN) {
		fprintf(stderr, "radv: Unknown family: %s\n", family);
		abort();
	}

	info->pci_id = radv_null_winsys_get_pci_id(info->family);
	info->max_se = 4;
	info->max_wave64_per_simd = info->family >= CHIP_POLARIS10 &&
				    info->family <= CHIP_VEGAM ? 8 : 10;

	if (info->chip_class >= GFX10)
		info->num_physical_sgprs_per_simd = 128 * info->max_wave64_per_simd * 2;
	else if (info->chip_class >= GFX8)
		info->num_physical_sgprs_per_simd = 800;
	else
		info->num_physical_sgprs_per_simd = 512;

	info->num_physical_wave64_vgprs_per_simd = info->chip_class >= GFX10 ? 512 : 256;
	info->num_simd_per_compute_unit = info->chip_class >= GFX10 ? 2 : 4;
	info->lds_size_per_workgroup = info->chip_class >= GFX10 ? 128 * 1024 : 64 * 1024;
}

static void radv_null_winsys_destroy(struct radeon_winsys *rws)
{
	FREE(rws);
}

struct radeon_winsys *
radv_null_winsys_create()
{
	struct radv_null_winsys *ws;

	ws = calloc(1, sizeof(struct radv_null_winsys));
	if (!ws)
		return NULL;

	ws->base.destroy = radv_null_winsys_destroy;
	ws->base.query_info = radv_null_winsys_query_info;
	radv_null_bo_init_functions(ws);
	radv_null_cs_init_functions(ws);

	return &ws->base;
}
