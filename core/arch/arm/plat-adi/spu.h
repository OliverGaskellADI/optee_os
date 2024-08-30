// SPDX-License-Identifier: BSD-2-Clause
#ifndef ADI_SPU_H
#define ADI_SPU_H
/**
 * The System Protection Unit is used to control accesses to peripherals
 * through the system crossbar. This driver allows other peripherals or
 * trusted applications to request changes to the security status of
 * system peripherals
 *
 * Copyright (c) 2023, Analog Devices, Inc.
 */

#define SPU_WP_CM0 (1 << 0)
#define SPU_WP_CM1 (1 << 1)
#define SPU_WP_CM2 (1 << 2)
#define SPU_WP_SM0 (1 << 16)
#define SPU_WP_SM1 (1 << 17)
#define SPU_WP_SM2 (1 << 18)
#define SPU_WP_ALL (SPU_WP_CM0 | SPU_WP_CM1 | SPU_WP_CM2 | SPU_WP_SM0 | SPU_WP_SM1 | SPU_WP_SM2)

/**
 * Enable the writeprotect for peripheral n (check the manual) using the core mask
 * given
 */
void spu_peripheral_writeprotect(uint32_t n, uint32_t mask);

/**
 * Enable secure-only options for the given peripheral n
 */
void spu_peripheral_secure(uint32_t n);

/**
 * Per platform initialization function called by the SPU driver
 */
void spu_platform_init(void);

#endif
