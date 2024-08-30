// SPDX-License-Identifier: BSD-2-Clause
/**
 * SMPU driver for ADI SC5xx SoCs
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
#include <util.h>

#include "smpu.h"

#define SMPU_REGION_REGS_SIZE	0x18
#define SMPU_MAX_REGIONS		8

#define SMPU_RCTL(n)		(0x20 + SMPU_REGION_REGS_SIZE*(n))
#define SMPU_RCTL_WPROTEN	BIT(10)
#define SMPU_RCTL_RPROTEN	BIT(8)
#define SMPU_RCTL_EN		BIT(0)

#define SMPU_RADDR(n)		(0x24 + SMPU_REGION_REGS_SIZE*(n))
#define SMPU_RIDA(n)		(0x28 + SMPU_REGION_REGS_SIZE*(n))
#define SMPU_RIDMASKA(n)	(0x2c + SMPU_REGION_REGS_SIZE*(n))
#define SMPU_RIDB(n)		(0x30 + SMPU_REGION_REGS_SIZE*(n))
#define SMPU_RIDMASKB(n)	(0x34 + SMPU_REGION_REGS_SIZE*(n))

#define SMPU_SECURERCTL(n)	(0x820 + 4*(n))

/**
 * We use the secure access settings to restrict regions based on the secure/insecure
 * state of the master accessing the region, rather than doing ID based comparisons
 * because the IDs do not extend to core secure/insecure options
 */
void smpu_configure_region(vaddr_t smpu_base, uint32_t id, uint32_t base,
	uint32_t size)
{
	uint32_t aligned_base;

	// Size must be a power of 2
	if (size & (size-1)) {
		EMSG("Invalid size 0x%x specified for smpu\n", size);
		panic();
	}

	// Regions are size-aligned, not page aligned!
	aligned_base = base & ~(size-1);
	if (base != aligned_base) {
		EMSG("SMPU region base must be aligned as a multiple of the region size\n");
		panic();
	}

	// ctz operation is equivalent to log2(n) for a power of 2 input, verified above
	// and size should be shifted such that 4096 = 2^12 is 0
	size = __builtin_ctz(size) - 12;

	if (id < SMPU_MAX_REGIONS) {
		DMSG("configuring region %u [0x%x, 0x%x] for secure-only transactions\n", id,
			base, base + (1 << (size + 12)) - 1);

		io_write32(smpu_base + SMPU_RADDR(id), base);
		// 0 = secure enabled, non-secure disabled
		io_write32(smpu_base + SMPU_SECURERCTL(id), 0);
		io_write32(smpu_base + SMPU_RCTL(id), (size << 1) | SMPU_RCTL_EN);
	}
}

/**
 * Region does not need to be a power of 2, but if we run out of SMPU regions
 * kill the system. This should be used for compound or not well-aligned mappings
 * because the SMPU requires strict size based alignment (i.e. 16 MB must be
 * 16 MB aligned, not 4 KB aligned)
 */
uint32_t smpu_configure_compound_region(vaddr_t smpu_base, uint32_t id,
	uint32_t base, uint32_t size)
{
	while (size && id < SMPU_MAX_REGIONS) {
		uint32_t rsize = (1 << __builtin_ctz(size));
		smpu_configure_region(smpu_base, id, base, rsize);
		id += 1;
		base += rsize;
		size -= rsize;
	}

	if (size) {
		DMSG("Could not configure SMPUs to cover the entire trusted memory region\n");
		DMSG("Please reassign it with better alignment\n");
		panic();
	}

	return id;
}

static TEE_Result smpu_init(void) {
	return smpu_platform_init();
}
early_init(smpu_init);
