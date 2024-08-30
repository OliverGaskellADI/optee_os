// SPDX-License-Identifier: BSD-2-Clause
#ifndef ADI_SMPU_H
#define ADI_SMPU_H

#include <types_ext.h>
#include <tee_api_types.h>

void smpu_configure_region(vaddr_t smpu_base, uint32_t id, uint32_t base,
	uint32_t size);

uint32_t smpu_configure_compound_region(vaddr_t smpu_base, uint32_t id,
	uint32_t base, uint32_t size);

TEE_Result smpu_platform_init(void);

#endif
