// SPDX-License-Identifier: BSD-2-Clause
/**
 * SMPU configuration for the SC598 and related SoCs
 *
 * Copyright (c) 2023, Analog Devices, Inc.
 */
#include <inttypes.h>
#include <kernel/panic.h>
#include <mm/core_mmu.h>
#include <mm/core_memprot.h>
#include <platform_config.h>
#include <stddef.h>
#include <util.h>

#include "../smpu.h"

register_phys_mem(MEM_AREA_IO_SEC, ADSP_SC5XX_SMPU9_BASE, ADSP_SC5XX_SMPU9_SIZE);

TEE_Result smpu_platform_init(void) {
	uint32_t tzbase, tzsize;
	uint32_t id;
	vaddr_t smpu_base = core_mmu_get_va(ADSP_SC5XX_SMPU9_BASE, MEM_AREA_IO_SEC,
		ADSP_SC5XX_SMPU9_SIZE);

	if (!smpu_base)
		panic();

	/*
	 * Peripheral restrictions are generally covered through the SPU
	 * So we only configure the SMPU for DMC0 (SMPU9) to cover restricted
	 * regions of DDR
	 */
	tzbase = TZDRAM_BASE;
	tzsize = TZDRAM_SIZE;
	id = 0;

	if (TZDRAM_BASE + TZDRAM_SIZE == CFG_TFAMEM_START) {
		tzsize += CFG_TFAMEM_SIZE;
	}
	else {
		id = smpu_configure_compound_region(smpu_base, id,
			CFG_TFAMEM_START, CFG_TFAMEM_SIZE);
	}

	smpu_configure_compound_region(smpu_base, id, tzbase, tzsize);
	return TEE_SUCCESS;
}
