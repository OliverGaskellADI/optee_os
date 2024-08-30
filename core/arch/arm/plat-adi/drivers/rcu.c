// SPDX-License-Identifier: BSD-2-Clause
/**
 * RCU driver for ADI SC5xx SoCs
 *
 * Copyright (c) 2023, Analog Devices, Inc.
 */

#include <inttypes.h>
#include <initcall.h>
#include <io.h>
#include <kernel/delay.h>
#include <kernel/panic.h>
#include <mm/core_mmu.h>
#include <mm/core_memprot.h>
#include <platform_config.h>
#include <trace.h>

#include <spu.h>

#include <adi_sec.h>
#include <adi_rcu.h>

#define ADI_RCU_STOP_TIMEOUT		10000000

/* Base register offsets */
#define ADI_RCU_REG_CTL				0x00
#define ADI_RCU_REG_STAT			0x04
#define ADI_RCU_REG_CRCTL			0x08
#define ADI_RCU_REG_CRSTAT			0x0c

// @todo move platform-specific options into a platform_flavor-based header
#ifdef CONFIG_ARCH_SC58X
#define ADI_RCU_REG_SIDIS			0x10
#define ADI_RCU_REG_SISTAT			0x14
#define ADI_RCU_REG_SVECT_LCK		0x18
#define ADI_RCU_REG_BCODE			0x1c
#define ADI_RCU_REG_SVECT0			0x20
#define ADI_RCU_REG_SVECT1			0x24
#define ADI_RCU_REG_SVECT2			0x28
#define ADI_RCU_REG_MSG				0x60
#define ADI_RCU_REG_MSG_SET			0x64
#define ADI_RCU_REG_MSG_CLR			0x68
#else
#define ADI_RCU_REG_SRRQSTAT		0x18
#define ADI_RCU_REG_SIDIS			0x1c
#define ADI_RCU_REG_SISTAT			0x20
#define ADI_RCU_REG_BCODE			0x28
#define ADI_RCU_REG_SVECT0			0x2c
#define ADI_RCU_REG_SVECT1			0x30
#define ADI_RCU_REG_SVECT2			0x34
#define ADI_RCU_REG_MSG				0x6c
#define ADI_RCU_REG_MSG_SET			0x70
#define ADI_RCU_REG_MSG_CLR			0x74
#endif

/* Register bit definitions */
#define ADI_RCU_CTL_SYSRST		BIT(0)

/* Bit values for the RCU0_MSG register */
#define RCU_MSG_C0IDLE			0x00000100		/* Core 0 Idle */
#define RCU_MSG_C1IDLE			0x00000200		/* Core 1 Idle */
#define RCU_MSG_C2IDLE			0x00000400		/* Core 2 Idle */
#define RCU_MSG_CRR0			0x00001000		/* Core 0 reset request */
#define RCU_MSG_CRR1			0x00002000		/* Core 1 reset request */
#define RCU_MSG_CRR2			0x00004000		/* Core 2 reset request */
#define RCU_MSG_C1ACTIVATE		0x00080000		/* Core 1 Activated */
#define RCU_MSG_C2ACTIVATE		0x00100000		/* Core 2 Activated */

/* Bit values for RCU_CRCTL */
#define RCU_CRCTL_CRES_ARM		BIT(0)
#define RCU_CRCTL_CRES_SHARC0	BIT(1)
#define RCU_CRCTL_CRES_SHARC1	BIT(2)

register_phys_mem(MEM_AREA_IO_SEC, ADSP_SC5XX_RCU_BASE, ADSP_SC5XX_RCU_SIZE);

static vaddr_t rcu_base __nex_bss;

void adi_rcu_reset(void) {
	uint32_t val = io_read32(rcu_base + ADI_RCU_REG_CTL);
	io_write32(rcu_base + ADI_RCU_REG_CTL, val | ADI_RCU_CTL_SYSRST);
}

void adi_rcu_set_svect(uint32_t sharc, vaddr_t svect) {
	if (sharc == 0)
		io_write32(rcu_base + ADI_RCU_REG_SVECT1, svect);
	else if (sharc == 1)
		io_write32(rcu_base + ADI_RCU_REG_SVECT2, svect);
}

static bool is_core_in_reset(uint32_t sharc) {
	return io_read32(rcu_base + ADI_RCU_REG_CRCTL) & (RCU_CRCTL_CRES_SHARC0 << sharc)
		? true : false;
}

static bool is_core_idle(uint32_t sharc) {
	return io_read32(rcu_base + ADI_RCU_REG_MSG) & (RCU_MSG_C1IDLE << sharc)
		? true : false;
}

/**
 * This is for SHARC cores only, so the coreid is the 0-indexed sharc number
 */
TEE_Result adi_rcu_stop_core(uint32_t sharc, uint32_t coreirq) {
	uint32_t loops = ADI_RCU_STOP_TIMEOUT;

	if (is_core_in_reset(sharc))
		return TEE_SUCCESS;

	if (!is_core_idle(sharc)) {
		// Set core reset request bit in RCU_MSG bit(12:14)
		io_write32(rcu_base + ADI_RCU_REG_MSG_SET, RCU_MSG_CRR1 << sharc);

		// Raise SOFT IRQ through SEC
		// DSP enter into ISR to release interrupts used by DSP program
		adi_sec_set_ssi_coreid(coreirq, sharc);
		adi_sec_enable_ssi(coreirq, false, true);
		adi_sec_enable_sci(sharc);
		adi_sec_raise_irq(coreirq);
	}

	// Wait until the specific core enter into IDLE bit(8:10)
	// DSP should set the IDLE bit to 1 manully in ISR
	while (loops && !is_core_idle(sharc)) {
		udelay(1);
		loops -= 1;
	}

	if (!loops) {
		EMSG("Timeout waiting for SHARC %d to IDLE\n", sharc);
	}

	// Clear core reset request bit in RCU_MSG bit(12:14)
	io_write32(rcu_base + ADI_RCU_REG_MSG_CLR, RCU_MSG_CRR1 << sharc);

	// Clear Activate bit when stop SHARC core
	io_write32(rcu_base + ADI_RCU_REG_MSG_CLR, RCU_MSG_C1ACTIVATE << sharc);

	return TEE_SUCCESS;
}

TEE_Result adi_rcu_reset_core(uint32_t sharc) {
	// First put core in reset.
    // Clear CRSTAT bit for given coreid.
	io_write32(rcu_base + ADI_RCU_REG_CRSTAT, RCU_CRCTL_CRES_SHARC0 << sharc);

	// Set SIDIS to disable the system interface
	io_setbits32(rcu_base + ADI_RCU_REG_SIDIS, 1 << sharc);

	// Wait for access to coreX have been disabled and all the pending
	// transactions have completed
	udelay(50);

	// Set CRCTL bit to put core in reset
	io_setbits32(rcu_base + ADI_RCU_REG_CRCTL, RCU_CRCTL_CRES_SHARC0 << sharc);

	// Poll until Core is in reset
	while (!(io_read32(rcu_base + ADI_RCU_REG_CRSTAT) & (RCU_CRCTL_CRES_SHARC0 << sharc)))
		;

	// Clear SIDIS to reenable the system interface
	io_clrbits32(rcu_base + ADI_RCU_REG_SIDIS, 1 << sharc);

	udelay(50);

	// Take Core out of reset
	io_clrbits32(rcu_base + ADI_RCU_REG_CRCTL, RCU_CRCTL_CRES_SHARC0 << sharc);

	// Wait for done
	udelay(50);

	return TEE_SUCCESS;
}

TEE_Result adi_rcu_start_core(uint32_t sharc) {
	// Clear the IDLE bit when start the SHARC core
	io_write32(rcu_base + ADI_RCU_REG_MSG_CLR, RCU_MSG_C1IDLE << sharc);

	// Notify CCES
	io_write32(rcu_base + ADI_RCU_REG_MSG_SET, RCU_MSG_C1ACTIVATE << sharc);

	return TEE_SUCCESS;
}

static TEE_Result adi_init_rcu(void) {
	rcu_base = core_mmu_get_va(ADSP_SC5XX_RCU_BASE, MEM_AREA_IO_SEC,
		ADSP_SC5XX_RCU_SIZE);

	if (!rcu_base)
		panic();

	spu_peripheral_secure(ADSP_SC5XX_SPUID_RCU);

	return TEE_SUCCESS;
}

driver_init(adi_init_rcu);
