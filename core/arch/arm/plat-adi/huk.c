// SPDX-License-Identifier: BSD-2-Clause
/**
 * HUK retrieval via OTP
 *
 * Copyright (c) 2022, Analog Devices, Inc.
 */

#include <assert.h>
#include <crypto/crypto.h>
#include <inttypes.h>
#include <kernel/panic.h>
#include <kernel/tee_common_otp.h>
#include <trace.h>

#include "otp.h"

#define ADI_OTP_HUK_BYTE_LEN 32

TEE_Result tee_otp_get_hw_unique_key(struct tee_hw_unique_key *hwkey)
{
	TEE_Result result;
	struct adi_otp *otp = adi_get_otp();
	uint8_t buffer[ADI_OTP_HUK_BYTE_LEN];
	uint32_t len = sizeof(buffer);
	bool zeroes = true;
	int i;

	result = adi_otp_read(otp, ADI_OTP_ID_huk, buffer, &len, ADI_OTP_ACCESS_SECURE);
	if (TEE_SUCCESS != result)
		return result;

	assert(sizeof(hwkey->data) <= sizeof(buffer));

	for (i = 0; i < ADI_OTP_HUK_BYTE_LEN; ++i) {
		if (0 != buffer[i]) {
			zeroes = false;
			break;
		}
	}

	if (zeroes) {
#ifdef CFG_ADI_AUTOGEN_HUK
		result = crypto_rng_read(buffer, sizeof(buffer));
		if (TEE_SUCCESS != result)
			panic("Could not read enough data from crypto RNG to initialize HUK!\n");

		adi_otp_write(otp, ADI_OTP_ID_huk, buffer, sizeof(buffer), ADI_OTP_ACCESS_SECURE);
#else
		EMSG("HUK OTP is programmed with zeroes--please program a real HUK or enable CFG_ADI_AUTOGEN_HUK\n");
#endif
	}

	memcpy(&hwkey->data[0], buffer, sizeof(hwkey->data));
	return TEE_SUCCESS;
}

