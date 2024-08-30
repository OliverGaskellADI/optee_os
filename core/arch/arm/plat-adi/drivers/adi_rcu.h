// SPDX-License-Identifier: BSD-2-Clause
#ifndef ADI_RCU_H
#define ADI_RCU_H
/**
 * The reset control unit is used to reset the platform as well as reset individual
 * cores, as required when code is loaded to a SHARC from another core
 *
 * Copyright (c) 2023, Analog Devices, Inc.
 */

void adi_rcu_reset(void);

/**
 * These are for SHARC cores only and start at 0 for SHARC 0
 */
void adi_rcu_set_svect(uint32_t sharc, vaddr_t svect);
TEE_Result adi_rcu_stop_core(uint32_t sharc, uint32_t coreirq);
TEE_Result adi_rcu_reset_core(uint32_t sharc);
TEE_Result adi_rcu_start_core(uint32_t sharc);

#endif
