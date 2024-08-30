// SPDX-License-Identifier: BSD-2-Clause
#ifndef ADI_SEC_H
#define ADI_SEC_H
/**
 * The SEC is used to manage system events and connect them with interrupts
 * that can be routed to the ARM core or SHARC cores
 *
 * Copyright (c) 2023, Analog Devices, Inc.
 */

#include <stdbool.h>
#include <stdint.h>

void adi_sec_raise_irq(unsigned int irq);
void adi_sec_enable_ssi(unsigned int sid, bool fault, bool source);
void adi_sec_enable_sci(uint32_t sharc);
void adi_sec_set_ssi_coreid(unsigned int sid, uint32_t sharc);

#endif
