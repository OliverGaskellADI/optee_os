// SPDX-License-Identifier: BSD-2-Clause
/**
 * SPU driver for ADI SC5xx SoCs
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

#include "spu.h"

#define SPU_SECUREP(n) (0xa00 + 4*(n))
#define SPU_SECUREP_MSEC (1 << 1)
#define SPU_SECUREP_SSEC (1 << 0)

#define SPU_WP(n) (0x400 + 4*(n))

// So far, constant on all SoCs
#define SPU_SECURECHK 		0x84c
#define SPU_SECUREC1		0x984
#define SPU_SECUREC2		0x988

register_phys_mem(MEM_AREA_IO_SEC, ADSP_SC5XX_SPU0_BASE, ADSP_SC5XX_SPU0_SIZE);

static vaddr_t spu0_base __nex_bss;

void spu_peripheral_writeprotect(uint32_t n, uint32_t mask) {
	if (n < SPU_WP_COUNT)
		io_write32(spu0_base + SPU_WP(n), mask);
	DMSG("Setting peripheral %u WP to 0x%x\n", n, mask);
}

void spu_peripheral_secure(uint32_t n) {
	if (n < SPU_SECUREP_COUNT)
		io_write32(spu0_base + SPU_SECUREP(n), SPU_SECUREP_MSEC | SPU_SECUREP_SSEC);
	DMSG("Setting peripheral %u to secure\n", n);
}

static TEE_Result init_spu(void) {
	uint32_t val;
	spu0_base = core_mmu_get_va(ADSP_SC5XX_SPU0_BASE, MEM_AREA_IO_SEC,
		ADSP_SC5XX_SPU0_SIZE);

	if (!spu0_base)
		panic();

	// Re-set SECUREC settings to prevent access to SHARC L1 by non-secure masters
	io_write32(spu0_base + SPU_SECUREC1, 1);
	io_write32(spu0_base + SPU_SECUREC2, 1);

	val = io_read32(spu0_base + SPU_SECURECHK);
	if (val != 0xffffffff)
		EMSG("OPTEE is not running as a secure master, chk = 0x%x!\n", val);

	spu_platform_init();
	return TEE_SUCCESS;
}

early_init(init_spu);
