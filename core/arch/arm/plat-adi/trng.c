
// SPDX-License-Identifier: BSD-2-Clause
/**
 * Based on the dra7 RNG driver
 *
 * Copyright (c) 2016, Linaro Limited
 * Copyright (c) 2022, ALPS ALPINE CO., LTD.
 * Copyright (c) 2023, Analog Devices, Inc.
 */

#include <initcall.h>
#include <io.h>
#include <kernel/panic.h>
#include <mm/core_memprot.h>
#include <rng_support.h>

#define TRNG_OUTPUT0		0x00
#define TRNG_OUTPUT1		0x04
#define TRNG_OUTPUT2		0x08
#define TRNG_OUTPUT3		0x0C
#define TRNG_STAT		0x10
# define TRNG_STAT_RDY		BIT(0)
# define TRNG_STAT_SHDNOVR	BIT(1)
#define TRNG_INTACK		0x10
#define TRNG_CTL		0x14
# define TRNG_CTL_TRNGEN	BIT(10)
#define TRNG_CFG		0x18
#define TRNG_ALMCNT		0x1C
#define TRNG_FROEN		0x20
#define TRNG_FRODETUNE		0x24
#define TRNG_ALMMSK		0x28
#define TRNG_ALMSTP		0x2C

#define TRNG_CTL_STARTUPCYC_SHIFT	16

#define TRNG_CFG_MAXREFCYC_SHIFT	16
#define TRNG_CFG_MINREFCYC_SHIFT	0

#define TRNG_ALMCNT_ALMTHRESH_SHIFT	0
#define TRNG_ALMCNT_SHDNTHRESH_SHIFT	16

#define TRNG_CTL_STARTUPCYC		0xff
#define TRNG_CFG_MINREFCYC		0x21
#define TRNG_CFG_MAXREFCYC		0x22
#define TRNG_ALMCNT_ALMTHRESH		0xff
#define TRNG_ALMCNT_SHDNTHRESH		0x4

#define TRNG_FROEN_FROS_MASK		GENMASK_32(7, 0)

register_phys_mem(MEM_AREA_IO_SEC, ADSP_SC5XX_TRNG0_BASE, ADSP_SC5XX_TRNG0_SIZE);

static vaddr_t rng __nex_bss;

TEE_Result hw_get_random_bytes(void* buf, size_t blen) {
	static int fifo_i;
	static uint32_t fifo[4];
	uint8_t ret;

	uint8_t *buffer = buf;
	size_t buffer_i = 0;

	while (buffer_i < blen) {
		if (!fifo_i) { // If we've exhausted the cached values, read more
			/* Is the result ready (available)? */
			while (!(io_read32(rng + TRNG_STAT) & TRNG_STAT_RDY)) {
				/* Is the shutdown threshold reached? */
				if (io_read32(rng + TRNG_STAT) & TRNG_STAT_SHDNOVR) {
					uint32_t alarm = io_read32(rng + TRNG_ALMSTP);
					uint32_t tune = io_read32(rng + TRNG_FRODETUNE);

					/* Clear the alarm events */
					io_write32(rng + TRNG_ALMMSK, 0x0);
					io_write32(rng + TRNG_ALMSTP, 0x0);
					/* De-tune offending FROs */
					io_write32(rng + TRNG_FRODETUNE, tune ^ alarm);
					/* Re-enable the shut down FROs */
					io_write32(rng + TRNG_FROEN, TRNG_FROEN_FROS_MASK);
					/* Clear the shutdown overflow event */
					io_write32(rng + TRNG_INTACK, TRNG_STAT_SHDNOVR);

					DMSG("Fixed FRO shutdown\n");
				}
			}
			/* Read random value */
			fifo[0] = io_read32(rng + TRNG_OUTPUT0);
			fifo[1] = io_read32(rng + TRNG_OUTPUT1);
			fifo[2] = io_read32(rng + TRNG_OUTPUT2);
			fifo[3] = io_read32(rng + TRNG_OUTPUT3);
			/* Acknowledge read complete */
			io_write32(rng + TRNG_INTACK, TRNG_STAT_RDY);
		}

		buffer[buffer_i++] = fifo[fifo_i];
		fifo_i = (fifo_i + 1) % 4;
	}

	return TEE_SUCCESS;
}

static TEE_Result sc59x_trng_init(void) {
	uint32_t val;

	rng = core_mmu_get_va(ADSP_SC5XX_TRNG0_BASE, MEM_AREA_IO_SEC,
		ADSP_SC5XX_TRNG0_SIZE);

	if (!rng)
		panic();

	/* Disable TRNG before configuring TRNG */
	io_write32(rng + TRNG_CTL, 0x0);

	/*
	 * Select the number of clock input cycles to the
	 * FROs between two samples
	 */
	val = 0;

	/* Ensure initial latency */
	val |= TRNG_CFG_MINREFCYC << TRNG_CFG_MINREFCYC_SHIFT;
	val |= TRNG_CFG_MAXREFCYC << TRNG_CFG_MAXREFCYC_SHIFT;
	io_write32(rng + TRNG_CFG, val);

	/* Configure the desired FROs */
	io_write32(rng + TRNG_FRODETUNE, 0x0);

	/* Enable all FROs */
	io_write32(rng + TRNG_FROEN, TRNG_FROEN_FROS_MASK);

	/*
	 * Select the maximum number of samples after
	 * which if a repeating pattern is still detected, an
	 * alarm event is generated
	 */
	val = TRNG_ALMCNT_ALMTHRESH << TRNG_ALMCNT_ALMTHRESH_SHIFT;

	/*
	 * Set the shutdown threshold to the number of FROs
	 * allowed to be shut downed
	 */
	val |= TRNG_ALMCNT_SHDNTHRESH << TRNG_ALMCNT_SHDNTHRESH_SHIFT;
	io_write32(rng + TRNG_ALMCNT, val);

	/* Enable the RNG module */
	val = TRNG_CTL_STARTUPCYC << TRNG_CTL_STARTUPCYC_SHIFT;
	val |= TRNG_CTL_TRNGEN;
	io_write32(rng + TRNG_CTL, val);

	IMSG("SC59x TRNG initialized");

	return TEE_SUCCESS;
}
early_init(sc59x_trng_init);
