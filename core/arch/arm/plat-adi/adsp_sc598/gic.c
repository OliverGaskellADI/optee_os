// SPDX-License-Identifier: BSD-2-Clause
/**
 * GICv3 configuration for SC598
 *
 * Copyright (c) 2023, Analog Devices, Inc.
 */
#include <drivers/gic.h>
#include <kernel/boot.h>
#include <kernel/panic.h>
#include <mm/core_mmu.h>
#include <mm/core_memprot.h>
#include <platform_config.h>
#include <trace.h>

register_phys_mem(MEM_AREA_IO_SEC, ADSP_SC5XX_GICD_BASE, ADSP_SC5XX_GIC_SIZE);

void main_init_gic(void)
{
	gic_init(0, ADSP_SC5XX_GICD_BASE);
}
