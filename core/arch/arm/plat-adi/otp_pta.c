// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright 2022, Analog Devices, Inc.
 */

#include <initcall.h>
#include <kernel/pseudo_ta.h>
#include <mm/core_mmu.h>
#include <mm/core_memprot.h>

#include "adi_otp_pta.h"
#include "otp.h"

#define PTA_NAME "adi_otp.pta"

static struct adi_otp __otp = { 0 };

/**
 * param 0: OTP ID to read
 * param 1: buffer to read into
 */
static TEE_Result cmd_read(void *session __unused, uint32_t param_types,
	TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t id;
	const uint32_t expected = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT,
		TEE_PARAM_TYPE_MEMREF_OUTPUT,
		TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE);

	if (param_types != expected) {
		DMSG("otp pta: param types mismatch\n");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	id = params[0].value.a;

	if (id >= __ADI_OTP_ID_COUNT) {
		DMSG("otp pta: id %d is too big\n", id);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	return adi_otp_read(&__otp, id, params[1].memref.buffer,
		&params[1].memref.size);
}

/**
 * param 0: OTP ID to write
 * param 1: buffer to write from
 */
static TEE_Result cmd_write(void *session __unused, uint32_t param_types,
	TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t id;
	const uint32_t expected = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT,
		TEE_PARAM_TYPE_MEMREF_INPUT,
		TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE);

	if (param_types != expected) {
		DMSG("otp pta: param types mismatch\n");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	id = params[0].value.a;

	if (id >= __ADI_OTP_ID_COUNT) {
		DMSG("otp pta: id %d is too big\n", id);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	return adi_otp_write(&__otp, id, params[1].memref.buffer,
		params[1].memref.size);
}

/**
 * param 0: OTP ID to invalidate
 */
static TEE_Result cmd_invalidate(void *session __unused, uint32_t param_types __unused,
	TEE_Param params[TEE_NUM_PARAMS] __unused)
{
	uint32_t id;
	const uint32_t expected = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT,
		TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE);

	if (param_types != expected) {
		DMSG("otp pta: param types mismatch\n");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	id = params[0].value.a;

	if (id >= __ADI_OTP_ID_COUNT) {
		DMSG("otp pta: id %d is too big\n", id);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	return adi_otp_invalidate(&__otp, id);
}

static TEE_Result invoke_command( void *session, uint32_t cmd, uint32_t param_types,
	TEE_Param params[TEE_NUM_PARAMS])
{
	switch (cmd) {
	case ADI_OTP_CMD_READ:
		return cmd_read(session, param_types, params);
	case ADI_OTP_CMD_WRITE:
		return cmd_write(session, param_types, params);
	case ADI_OTP_CMD_INVALIDATE:
		return cmd_invalidate(session, param_types, params);
	default:
		DMSG("otp pta: received invalid command %d\n", cmd);
		return TEE_ERROR_BAD_PARAMETERS;
	}
}

/* Addresses are SC598-specific */
#define ROM_OTP_BASE_ADDR              0x24000000
#define ROM_OTP_CONTROL_ADDR           0x31011000

#define ROM_OTP_SIZE                   0x2000
#define ROM_OTP_CONTROL_SIZE           0x100

register_phys_mem_pgdir(MEM_AREA_IO_SEC, ROM_OTP_BASE_ADDR, ROM_OTP_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_SEC, ROM_OTP_CONTROL_ADDR, ROM_OTP_CONTROL_SIZE);

static TEE_Result adi_otp_init(void) {
	__otp.control_base = core_mmu_get_va(ROM_OTP_CONTROL_ADDR, MEM_AREA_IO_SEC,
		ROM_OTP_CONTROL_SIZE);
	__otp.otp_rom_base = core_mmu_get_va(ROM_OTP_BASE_ADDR, MEM_AREA_IO_SEC,
		ROM_OTP_CONTROL_SIZE);
	return TEE_SUCCESS;
}
early_init(adi_otp_init);

pseudo_ta_register(.uuid = PTA_ADI_OTP_UUID, .name = PTA_NAME,
		   .flags = PTA_DEFAULT_FLAGS | TA_FLAG_DEVICE_ENUM,
		   .invoke_command_entry_point = invoke_command);
