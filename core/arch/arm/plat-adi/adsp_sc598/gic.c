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

static struct gic_data gic_data __nex_bss;

register_phys_mem(MEM_AREA_IO_SEC, ADSP_SC5XX_GICD_BASE, ADSP_SC5XX_GIC_SIZE);

void itr_core_handler(void) {
	gic_it_handle(&gic_data);
}

void main_init_gic(void) {
	vaddr_t gicd_base;

	gicd_base = core_mmu_get_va(ADSP_SC5XX_GICD_BASE, MEM_AREA_IO_SEC,
		ADSP_SC5XX_GIC_SIZE);

	if (!gicd_base)
		panic();

	/* Initialize GIC */
	gic_init(&gic_data, 0, gicd_base);
	itr_init(&gic_data.chip);
}
