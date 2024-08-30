// SPDX-License-Identifier: BSD-2-Clause
/**
 * TRU driver for ADI SC5xx SoCs
 *
 * Copyright (c) 2023, Analog Devices, Inc.
 */

#include <inttypes.h>
#include <initcall.h>
#include <io.h>
#include <kernel/panic.h>
#include <mm/core_mmu.h>
#include <mm/core_memprot.h>
#include <platform_config.h>
#include <trace.h>

#include "adi_tru.h"
#include <spu.h>

// @todo: platform flavor header parameterization
#define ADSP_SC5XX_TRU_MAX_MASTER_ID 182
#define ADSP_SC5XX_TRU_MAX_SLAVE_ID 187

#define ADSP_SC5XX_TRU_MASTER_SOFT3		134
#define ADSP_SC5XX_TRU_MASTER_SOFT4		135
#define ADSP_SC5XX_TRU_MASTER_SOFT5		136

#define ADSP_SC5XX_TRU_SLAVE_IRQ3		160
#define ADSP_SC5XX_TRU_SLAVE_IRQ7		164
#define ADSP_SC5XX_TRU_SLAVE_IRQ11		168

#define ADI_TRU_REG_GCTL		0x7f4
#define ADI_TRU_REG_MTR			0x7e0

register_phys_mem(MEM_AREA_IO_SEC, ADSP_SC5XX_TRU_BASE, ADSP_SC5XX_TRU_SIZE);

static vaddr_t tru_base __nex_bss;

void adi_tru_connect(uint32_t master, uint32_t slave) {
	if (slave > ADSP_SC5XX_TRU_MAX_SLAVE_ID) {
		EMSG("Tried to connect invalid slave id %d > %d\n", slave,
			ADSP_SC5XX_TRU_MAX_SLAVE_ID);
		return;
	}

	if (master > ADSP_SC5XX_TRU_MAX_MASTER_ID || master == 0) {
		EMSG("Tried to connect invalid master id %d > %d or 0\n", master,
			ADSP_SC5XX_TRU_MAX_MASTER_ID);
		return;
	}

	DMSG("Connect master %d to slave %d\n", master, slave);
	io_write32(tru_base + (slave*4), master);
}

void adi_tru_trigger(uint32_t master) {
	if (master > ADSP_SC5XX_TRU_MAX_MASTER_ID || master == 0) {
		EMSG("Tried to trigger invalid master id %d > %d or 0\n", master,
			ADSP_SC5XX_TRU_MAX_MASTER_ID);
		return;
	}

	io_write32(tru_base + ADI_TRU_REG_MTR, master);
}

bool adi_tru_is_ns_trigger_permitted(uint32_t master) {
	switch (master) {
	case ADSP_SC5XX_TRU_MASTER_SOFT4:
	case ADSP_SC5XX_TRU_MASTER_SOFT5:
		return true;
	default:
		return false;
	}
}

static TEE_Result adi_init_tru(void) {

	tru_base = core_mmu_get_va(ADSP_SC5XX_TRU_BASE, MEM_AREA_IO_SEC,
		ADSP_SC5XX_TRU_SIZE);

	if (!tru_base)
		panic();

	// Enable TRU
	io_write32(tru_base + ADI_TRU_REG_GCTL, 1);

	// @todo some platform flavor init link to set initial connections
	adi_tru_connect(ADSP_SC5XX_TRU_MASTER_SOFT3, ADSP_SC5XX_TRU_SLAVE_IRQ3);
	adi_tru_connect(ADSP_SC5XX_TRU_MASTER_SOFT4, ADSP_SC5XX_TRU_SLAVE_IRQ7);
	adi_tru_connect(ADSP_SC5XX_TRU_MASTER_SOFT5, ADSP_SC5XX_TRU_SLAVE_IRQ11);

	spu_peripheral_secure(120);

	return TEE_SUCCESS;
}

driver_init(adi_init_tru);
