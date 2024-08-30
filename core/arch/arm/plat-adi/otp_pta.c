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
#include "spu.h"

#define PTA_NAME "adi_otp.pta"

/*
 * If a _ALL config is defined, enable secure tier access for that option. Otherwise
 * use non-secure access
 */
#ifndef CFG_ADI_OTP_READ_ALL
	#define ADI_OTP_READ_SECURITY ADI_OTP_ACCESS_NONSECURE
#else
	#define ADI_OTP_READ_SECURITY ADI_OTP_ACCESS_SECURE
#endif

#ifndef CFG_ADI_OTP_WRITE_ALL
	#define ADI_OTP_WRITE_SECURITY ADI_OTP_ACCESS_NONSECURE
#else
	#define ADI_OTP_WRITE_SECURITY ADI_OTP_ACCESS_SECURE
#endif

#ifndef CFG_ADI_OTP_LOCK_ALL
	#define ADI_OTP_LOCK_SECURITY ADI_OTP_ACCESS_NONSECURE
#else
	#define ADI_OTP_LOCK_SECURITY ADI_OTP_ACCESS_SECURE
#endif

#ifndef CFG_ADI_OTP_INVALIDATE_ALL
	#define ADI_OTP_INVALIDATE_SECURITY ADI_OTP_ACCESS_NONSECURE
#else
	#define ADI_OTP_INVALIDATE_SECURITY ADI_OTP_ACCESS_SECURE
#endif

#ifndef CFG_ADI_OTP_IS_VALID_ALL
	#define ADI_OTP_IS_VALID_SECURITY ADI_OTP_ACCESS_NONSECURE
#else
	#define ADI_OTP_IS_VALID_SECURITY ADI_OTP_ACCESS_SECURE
#endif

#ifndef CFG_ADI_OTP_IS_WRITTEN_ALL
	#define ADI_OTP_IS_WRITTEN_SECURITY ADI_OTP_ACCESS_NONSECURE
#else
	#define ADI_OTP_IS_WRITTEN_SECURITY ADI_OTP_ACCESS_SECURE
#endif

static struct adi_otp __otp = { 0 };

struct adi_otp *adi_get_otp(void) {
	return &__otp;
}

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
		&params[1].memref.size, ADI_OTP_READ_SECURITY);
}

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

	if (ADI_OTP_ID_lock == id) {
		DMSG("otp pta: not safe to write arbitrary values to lock field. Use lock command\n");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	return adi_otp_write(&__otp, id, params[1].memref.buffer,
		params[1].memref.size, ADI_OTP_WRITE_SECURITY);
}

static TEE_Result cmd_lock(void *session __unused, uint32_t param_types,
	TEE_Param params[TEE_NUM_PARAMS] __unused)
{
	const uint32_t expected = TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE);
	uint32_t lock = 0x01;

	if (param_types != expected) {
		DMSG("otp pta: param types mismatch\n");
	}

	return adi_otp_write(&__otp, ADI_OTP_ID_lock, &lock, sizeof(lock),
		ADI_OTP_LOCK_SECURITY);
}

static TEE_Result cmd_invalidate(void *session __unused, uint32_t param_types,
	TEE_Param params[TEE_NUM_PARAMS])
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

	return adi_otp_invalidate(&__otp, id, ADI_OTP_INVALIDATE_SECURITY);
}

static TEE_Result cmd_is_valid(void *session __unused, uint32_t param_types,
	TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t id;
	const uint32_t expected = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT,
		TEE_PARAM_TYPE_VALUE_OUTPUT,
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

	return adi_otp_is_valid(&__otp, id, ADI_OTP_IS_VALID_SECURITY,
		&params[1].value.a);
}

static TEE_Result cmd_is_written(void *session __unused, uint32_t param_types,
	TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t id;
	const uint32_t expected = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT,
		TEE_PARAM_TYPE_VALUE_OUTPUT,
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

	return adi_otp_is_written(&__otp, id, ADI_OTP_IS_WRITTEN_SECURITY,
		&params[1].value.a);
}

static TEE_Result cmd_version(void *session __unused, uint32_t param_types,
	TEE_Param params[TEE_NUM_PARAMS])
{
	const uint32_t expected = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT,
		TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE);

	if (param_types != expected) {
		DMSG("otp pta: param types mismatch\n");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	params[0].value.a = adi_otp_major();
	params[0].value.b = adi_otp_minor();

	return TEE_SUCCESS;
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
	case ADI_OTP_CMD_VERSION:
		return cmd_version(session, param_types, params);
	case ADI_OTP_CMD_LOCK:
		return cmd_lock(session, param_types, params);
	case ADI_OTP_CMD_IS_VALID:
		return cmd_is_valid(session, param_types, params);
	case ADI_OTP_CMD_IS_WRITTEN:
		return cmd_is_written(session, param_types, params);
	default:
		DMSG("otp pta: received invalid command %d\n", cmd);
		return TEE_ERROR_BAD_PARAMETERS;
	}
}

/* All values are SC598-specific */
#define ROM_OTP_BASE_ADDR              0x24000000
#define ROM_OTP_CONTROL_ADDR           0x31011000

#define ROM_OTP_SIZE                   0x2000
#define ROM_OTP_CONTROL_SIZE           0x1000

#define SPU_SECUREP_OTP_MMR            57
#define SPU_SECUREP_SMPU_OTP           58

register_phys_mem(MEM_AREA_IO_SEC, ROM_OTP_BASE_ADDR, ROM_OTP_SIZE);
register_phys_mem(MEM_AREA_IO_SEC, ROM_OTP_CONTROL_ADDR, ROM_OTP_CONTROL_SIZE);

static TEE_Result adi_otp_init(void) {
	__otp.control_base = core_mmu_get_va(ROM_OTP_CONTROL_ADDR, MEM_AREA_IO_SEC,
		ROM_OTP_CONTROL_SIZE);
	__otp.otp_rom_base = core_mmu_get_va(ROM_OTP_BASE_ADDR, MEM_AREA_IO_SEC,
		ROM_OTP_SIZE);

	if (adi_otp_major() != ADI_OTP_MAJOR ||
		adi_otp_minor() != ADI_OTP_MINOR) {
		EMSG("OTP Library version mismatch, please rebuild OP-TEE!\n");
	}

	return TEE_SUCCESS;
}
early_init(adi_otp_init);

/**
 * This depends on spu and smpu init being completed in the early stage
 */
static TEE_Result adi_otp_secure_init(void) {
	spu_peripheral_secure(SPU_SECUREP_OTP_MMR);
	spu_peripheral_secure(SPU_SECUREP_SMPU_OTP);

	return TEE_SUCCESS;
}
early_init_late(adi_otp_secure_init);

pseudo_ta_register(.uuid = PTA_ADI_OTP_UUID, .name = PTA_NAME,
		   .flags = PTA_DEFAULT_FLAGS | TA_FLAG_DEVICE_ENUM,
		   .invoke_command_entry_point = invoke_command);
