// SPDX-License-Identifier: BSD-2-Clause
/**
 * SPU configuration for the SC598 and related SoCs
 *
 * Copyright (c) 2023, Analog Devices, Inc.
 */
#include <inttypes.h>
#include <platform_config.h>
#include <stddef.h>
#include <util.h>

#include "../spu.h"

/**
 * @todo Consider:
 * - SMPU L2-related registers; these should only be configurable on sharc possibly?
 * - More of the debug registers need to be covered to prevent enabling them
 */
void spu_platform_init(void) {
	size_t i;

	/**
	 * Note that the CGU, CDU, and PLL are WP only so that
	 * they can be read by the nonsecure components
	 * Notes:
	 * - Crypto PKP includes TRNG and PKA modules so we centralize the crypto
	 *   settings to avoid issues with which specific driver is responsible,
	 *   between the TRNG driver and the crypto driver, as either or both may
	 *   not be enabled in a given build
	 */
	uint32_t securep_ids[] = {
		109, // DMC0
		121, // SPU0
		126, // DPM0
		129, 130, 131, 132, 133, 134, // System Watchpoint Units
		139, 140, // SWU and SMPU for DMC0
		178, 179, // Crypto SPE and PKP
		181, // DAP ROM
		182, 183, 184, // SHARC0 DBG, CTI, PTM
		186, 187, 188, // SHARC1 DBG, CTI, PTM
		201, // TAPC
		202, // Debug Control
		203, 204, 205, 206, // System Watchpoint Units
	};

	/**
	 * If these are not also secured, they can still be read by the open world
	 * @todo linux needs some read-only support to be able to lock cgu/cdu regs
	 * so those have been left out until the linux driver is fixed or the system
	 * won't boot, permissible during development only
	 */
	uint32_t wp_ids[] = {
		109, // DMC0
		127, // PLL0
		128, // PLL1
	};

	// Initially, all peripherals are marked to support non-secure transactions
	// Here, mark the core protected peripherals as secure; other drivers can
	// also protect their peripherals with the SPU api separately
	for (i = 0; i < ARRAY_SIZE(securep_ids); ++i) {
		spu_peripheral_secure(securep_ids[i]);
	}

	for (i = 0; i < ARRAY_SIZE(wp_ids); ++i) {
		spu_peripheral_writeprotect(wp_ids[i], SPU_WP_ALL);
	}

}
