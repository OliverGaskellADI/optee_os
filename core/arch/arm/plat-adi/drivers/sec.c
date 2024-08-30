// SPDX-License-Identifier: BSD-2-Clause
/**
 * SEC driver for ADI SC5xx SoCs
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

#include <spu.h>
#include <adi_sec.h>

/* Global Registers */
#define ADI_SEC_REG_GCTL			0x000
#define ADI_SEC_REG_GSTAT			0x004
#define ADI_SEC_REG_RAISE			0x008
#define ADI_SEC_REG_END				0x00c

/* Fault management interface (SFI) registers */
#define ADI_SEC_REG_FCTL			0x010
#define ADI_SEC_REG_FSTAT			0x014
#define ADI_SEC_REG_FSID			0x018
#define ADI_SEC_REG_FEND			0x01c
#define ADI_SEC_REG_FDLY			0x020
#define ADI_SEC_REG_FDLY_CUR		0x024
#define ADI_SEC_REG_FSRDLY			0x028
#define ADI_SEC_REG_FSRDLY_CUR		0x02c
#define ADI_SEC_REG_FCOPP			0x030
#define ADI_SEC_REG_FCOPP_CUR		0x034

/* Start of CCTL registers */
#define ADI_SEC_REG_CCTL_BASE		0x400
#define ADI_SEC_CCTL_SIZE			0x040

#define ADI_SEC_REG_CCTL1			0x440
#define ADI_SEC_REG_CCTL2			0x480

/* Start of SCTL registesr */
#define ADI_SEC_REG_SCTL_BASE		0x800

/* Register bits */
#define ADI_SEC_CCTL_EN				0x00000001		/* SCI Enable */

#define ADI_SEC_SCTL_SRC_EN         0x00000004    /* SEN: Enable */
#define ADI_SEC_SCTL_FAULT_EN       0x00000002    /* FEN: Enable */
#define ADI_SEC_SCTL_INT_EN         0x00000001    /* IEN: Enable */

#define ADI_SEC_SCTL_CTG			0x0F000000    /* Core Target Select */

register_phys_mem(MEM_AREA_IO_SEC, ADSP_SC5XX_SEC_BASE, ADSP_SC5XX_SEC_SIZE);

static vaddr_t sec_base __nex_bss;

void adi_sec_raise_irq(unsigned int irq) {
	unsigned int sid = irq - 32;
	io_write32(sec_base + ADI_SEC_REG_RAISE, sid);
}

void adi_sec_enable_ssi(unsigned int sid, bool fault, bool source) {
	uint32_t set = 0;
	uint32_t offset;

	offset = ADI_SEC_REG_SCTL_BASE + 8*sid;

	if (fault)
		set |= ADI_SEC_SCTL_FAULT_EN;
	else
		set |= ADI_SEC_SCTL_INT_EN;

	if (source)
		set |= ADI_SEC_SCTL_SRC_EN;

	io_setbits32(sec_base + offset, set);
}

void adi_sec_enable_sci(uint32_t sharc) {
	uint32_t coreid = sharc + 1;
	uint32_t offset;

	offset = ADI_SEC_REG_CCTL_BASE + coreid*ADI_SEC_CCTL_SIZE;
	io_setbits32(sec_base + offset, ADI_SEC_CCTL_EN);
}

void adi_sec_set_ssi_coreid(unsigned int sid, uint32_t sharc) {
	uint32_t coreid = sharc + 1;
	uint32_t val;
	uint32_t offset;

	offset = ADI_SEC_REG_SCTL_BASE + 8*sid;

	val = io_read32(sec_base + offset);
	val &= ~ADI_SEC_SCTL_CTG;
	val |= ((coreid << 24) & ADI_SEC_SCTL_CTG);
	io_write32(sec_base + offset, val);
}

static TEE_Result adi_init_sec(void) {
	sec_base = core_mmu_get_va(ADSP_SC5XX_SEC_BASE, MEM_AREA_IO_SEC,
		ADSP_SC5XX_SEC_SIZE);

	if (!sec_base)
		panic();

	spu_peripheral_secure(ADSP_SC5XX_SPUID_SEC);

	return TEE_SUCCESS;
}

driver_init(adi_init_sec);
