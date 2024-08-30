// SPDX-License-Identifier: BSD-2-Clause
/**
 * ARM PSCI support for 32-bit ADI SC5xx SoCs
 * Copyright (c) 2023, Analog Devices, Inc.
 */

#include <sm/psci.h>

#include <adi_rcu.h>

int psci_features(uint32_t psci_fid) {
	switch(psci_fid) {
	case PSCI_PSCI_FEATURES:
	case PSCI_VERSION:
	case PSCI_SYSTEM_RESET:
		return PSCI_RET_SUCCESS;
	default:
		return PSCI_RET_NOT_SUPPORTED;
	}
}

uint32_t psci_version(void) {
	return PSCI_VERSION_1_0;
}

void __noreturn psci_system_reset(void) {
	adi_rcu_reset();
}
