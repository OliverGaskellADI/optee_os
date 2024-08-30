// SPDX-License-Identifier: BSD-2-Clause
/**
 * Implement additional fast SMCs supported by the ADSP SC5xx SoCs
 *
 * Copyright (c) 2023, Analog Devices, Inc.
 */

#include <sm/optee_smc.h>
#include <optee_msg.h>
#include <tee/entry_fast.h>

#include <adi_tru.h>

// Unused in the Trusted OS calls range
#define OPTEE_SMC_OWNER_ADI U(51)

#define ADI_SMC_FUNCID_TRU_TRIGGER 0x00

#define ADI_SMC_TRU_TRIGGER \
	OPTEE_SMC_CALL_VAL(OPTEE_SMC_32, OPTEE_SMC_FAST_CALL, \
		OPTEE_SMC_OWNER_ADI, ADI_SMC_FUNCID_TRU_TRIGGER)

void tee_entry_fast(struct thread_smc_args *args) {
	switch (args->a0) {
	case ADI_SMC_TRU_TRIGGER:
		args->a0 = OPTEE_SMC_RETURN_ENOTAVAIL;
#ifdef CFG_ADI_TRU
		if (adi_tru_is_ns_trigger_permitted(args->a1)) {
			adi_tru_trigger(args->a1);
			args->a0 = OPTEE_SMC_RETURN_OK;
		}
#endif
		break;
	default:
		__tee_entry_fast(args);
		break;
	}
}
