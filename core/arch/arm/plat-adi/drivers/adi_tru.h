// SPDX-License-Identifier: BSD-2-Clause
#ifndef ADI_TRU_H
#define ADI_TRU_H
/**
 * The trigger routing unit is used to route events from sources to other modules
 * that should be activated by those triggers. It can also generate a trigger from
 * a particular source via software
 *
 * Copyright (c) 2023, Analog Devices, Inc.
 */

/**
 * Connect the specified trigger master (source) to the given sink (slave)
 */
void adi_tru_connect(uint32_t master, uint32_t slave);

/**
 * Generate a software activation of the specified master
 */
void adi_tru_trigger(uint32_t master);

/**
 * Determine if this master may be software triggered from the nonsecure domain
 */
bool adi_tru_is_ns_trigger_permitted(uint32_t master);

#endif
