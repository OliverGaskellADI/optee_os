// SPDX-License-Identifier: BSD-2-Clause
/**
 * SHARC load/start/stop/reset driver and pTA for ADI SC5xx SoCs
 * Copyright (c) 2023, Analog Devices, Inc.
 */

#include <assert.h>
#include <crypto/crypto.h>
#include <inttypes.h>
#include <initcall.h>
#include <io.h>
#include <kernel/panic.h>
#include <kernel/pseudo_ta.h>
#include <mm/core_mmu.h>
#include <mm/core_memprot.h>
#include <platform_config.h>
#include <string.h>
#include <trace.h>

#include <spu.h>
#include <adi_rcu.h>
#include "adi_sharc_pta.h"
#include <otp.h>

#define PTA_NAME "adi_sharc.pta"

#define OTP_PUBLIC_KEY_LEN 64

register_phys_mem(MEM_AREA_IO_SEC, ADSP_SC5XX_SHARC0_L1_BASE, ADSP_SC5XX_SHARC0_L1_SIZE);
register_phys_mem(MEM_AREA_IO_SEC, ADSP_SC5XX_SHARC1_L1_BASE, ADSP_SC5XX_SHARC1_L1_SIZE);
register_phys_mem(MEM_AREA_IO_SEC, ADSP_SC5XX_L2_BASE, ADSP_SC5XX_L2_SIZE);

enum core_state {
	SHARC_CORE_STOPPED = 0,
	SHARC_CORE_RUNNING,
};

struct sharc_state {
	enum core_state state;
	vaddr_t l1_base;
	uint32_t l1_load_base;
	uint32_t l1_size;
	uint32_t load_addr;
	uint32_t irq;
};

static vaddr_t l2_base __nex_bss;
static struct sharc_state state[ADSP_SC5XX_NUM_SHARC_CORES] __nex_bss;

struct bcode_flag_t {
	uint32_t bCode:4,			/* 0-3 */
			 bFlag_save:1,		/* 4 */
			 bFlag_aux:1,		/* 5 */
			 bReserved:1,		/* 6 */
			 bFlag_forward:1,	/* 7 */
			 bFlag_fill:1,		/* 8 */
			 bFlag_quickboot:1, /* 9 */
			 bFlag_callback:1,	/* 10 */
			 bFlag_init:1,		/* 11 */
			 bFlag_ignore:1,	/* 12 */
			 bFlag_indirect:1,	/* 13 */
			 bFlag_first:1,		/* 14 */
			 bFlag_final:1,		/* 15 */
			 bHdrCHK:8,			/* 16-23 */
			 bHdrSIGN:8;		/* 0xAD, 0xAC or 0xAB */
};

struct ldr_hdr {
	union {
		struct bcode_flag_t bcode_flag;
		uint32_t bcode_int;
	};
	uint32_t target_addr;
	uint32_t byte_count;
	uint32_t argument;
};

struct sb_attr {
	uint32_t id;
	uint32_t value;
};

struct sb_hdr {
	uint32_t type;
	uint8_t signature[64];
	uint8_t hash[32];
	uint8_t key[24];
	uint8_t iv[16];
	uint32_t length;
	struct sb_attr attributes[8];
	uint8_t padding[48];
};

#define ADSP_SECURE_BOOT_EXPECTED_HEADER_LEN 256

/**
 * No primitives like this available in io.h
 */
static void io_memset(vaddr_t vdst, uint8_t byte, size_t count) {
	uint32_t bigbyte = 0x01010101 * byte;

	// Align address first
	while ((vdst & 0x3) && count) {
		io_write8(vdst, byte);
		vdst += 1;
		count -= 1;
	}

	// Handle bulk memsets
	while (count >= sizeof(uint32_t)) {
		io_write32(vdst, bigbyte);
		count -= sizeof(uint32_t);
		vdst += sizeof(uint32_t);
	}

	// Handle leftover bytes
	while (count) {
		io_write8(vdst, byte);
		vdst += 1;
		count -= 1;
	}
}

static void io_memcpy(vaddr_t vdst, void *src, size_t count) {
	vaddr_t vsrc = (vaddr_t) src;
	uint8_t *src8;
	uint32_t *src32;

	if ((vdst & 0x3) != (vsrc & 0x3)) {
		EMSG("Misaligned io_memcpy call copying %p to 0x%lx\n", src, vdst);
	}

	while ((vsrc & 0x3) && count) {
		src8 = (uint8_t *) vsrc;
		io_write8(vdst, *src8);
		vdst += 1;
		count -= 1;
		vsrc += 1;
	}

	src32 = (uint32_t *) vsrc;
	while (count >= sizeof(uint32_t)) {
		io_write32(vdst, *src32);
		vdst += sizeof(uint32_t);
		count -= sizeof(uint32_t);
		src32 += 1;
	}

	src8 = (uint8_t *) src32;
	while (count) {
		io_write8(vdst, *src8);
		vdst += 1;
		count -= 1;
		src8 += 1;
	}
}

/**
 * Confirm that a secure header we understand and verify that the image length
 * reported matches the size of the buffer provided
 */
static TEE_Result check_secure_header(void *data, size_t size) {
	struct sb_hdr *hdr;
	uint32_t type;
	uint32_t length;
	uintptr_t data_end = (uintptr_t) data + size;

	if (sizeof(struct sb_hdr) != ADSP_SECURE_BOOT_EXPECTED_HEADER_LEN) {
		EMSG("The size of struct sb_hdr is not 256; there is a problem "
			"with the struct padding\n");
		return TEE_ERROR_CANCEL;
	}

	hdr = data;
	if ((uintptr_t) (hdr + 1) > data_end)
		return TEE_ERROR_SHORT_BUFFER;

	type = TEE_U32_BSWAP(hdr->type);
	switch (type) {
		case 0x42427803: // BLx
		case 0x424c7703: // BLw
			return TEE_ERROR_NOT_SUPPORTED;
		case 0x424c7003:
			DMSG("Found a BLp header\n");
			break;
		default:
			DMSG("Secure boot header type invalid\n");
			return TEE_ERROR_BAD_FORMAT;
	}

	length = TEE_U32_BSWAP(hdr->length);
	if (length + sizeof(struct sb_hdr) != size) {
		DMSG("0x%x + 0x%x != 0x%x, secure boot header length mismatch\n",
			length, sizeof(struct sb_hdr), size);
		return TEE_ERROR_SHORT_BUFFER;
	}

	return TEE_SUCCESS;
}

static TEE_Result find_hash_type(struct sb_hdr *hdr, uint32_t *type) {
	size_t i;

	for (i = 0; i < 8; ++i) {
		uint32_t id = TEE_U32_BSWAP(hdr->attributes[i].id);
		uint32_t value = TEE_U32_BSWAP(hdr->attributes[i].value);

		if (!id)
			continue;

		DMSG("Found hdr attribute 0x%x\n", id);

		if (id == 0x80000003) {
			if (value == 224) {
				*type = TEE_ALG_SHA224;
				return TEE_SUCCESS;
			}

			if (value == 256) {
				*type = TEE_ALG_SHA256;
				return TEE_SUCCESS;
			}

			DMSG("Invalid hash size %u\n", value);
			return TEE_ERROR_BAD_FORMAT;
		}
	}

	DMSG("Could not find an ECDSA type attribute in secure header\n");
	return TEE_ERROR_BAD_FORMAT;
}

static TEE_Result __signature_verify(uint32_t key_id, struct sb_hdr *hdr,
	uint8_t digest[TEE_SHA256_HASH_SIZE], size_t digest_size)
{
	struct adi_otp *otp;
	struct ecc_public_key pk;
	uint8_t otp_pk[OTP_PUBLIC_KEY_LEN];
	uint32_t otp_len = OTP_PUBLIC_KEY_LEN;
	uint32_t otp_result;
	TEE_Result ret;

	otp = adi_get_otp();

	ret = adi_otp_is_written(otp, key_id, ADI_OTP_ACCESS_SECURE, &otp_result);
	if (TEE_SUCCESS != ret)
		return ret;
	if (otp_result != 1)
		return TEE_ERROR_NO_DATA;

	ret = adi_otp_is_valid(otp, key_id, ADI_OTP_ACCESS_SECURE, &otp_result);
	if (TEE_SUCCESS != ret)
		return ret;
	if (otp_result != 1)
		return TEE_ERROR_NO_DATA;

	ret = adi_otp_read(otp, key_id, otp_pk, &otp_len, ADI_OTP_ACCESS_SECURE);
	if (TEE_SUCCESS != ret)
		return ret;

	if (otp_len != OTP_PUBLIC_KEY_LEN) {
		EMSG("OTP key length mismatch in implementation\n");
		return TEE_ERROR_CANCEL;
	}

	ret = crypto_acipher_alloc_ecc_public_key(&pk, TEE_TYPE_ECDSA_PUBLIC_KEY, 256);
	if (TEE_SUCCESS != ret)
		return ret;

	pk.curve = TEE_ECC_CURVE_NIST_P256;

	ret = crypto_bignum_bin2bn(otp_pk, OTP_PUBLIC_KEY_LEN/2, pk.x);
	if (TEE_SUCCESS != ret)
		goto cleanup_pk;

	ret = crypto_bignum_bin2bn(otp_pk + OTP_PUBLIC_KEY_LEN/2, OTP_PUBLIC_KEY_LEN/2,
		pk.y);
	if (TEE_SUCCESS != ret)
		goto cleanup_pk;

	/*
	 * Although we may be doing "ECDSA 224" or "ECDSA 256" this refers only to
	 * the SHA size, the underlying key is always a prime256 key, so the signature
	 * is always 64 bytes
	 */
	ret = crypto_acipher_ecc_verify(TEE_ALG_ECDSA_P256, &pk, digest, digest_size,
		hdr->signature, 64);

cleanup_pk:
	crypto_acipher_free_ecc_public_key(&pk);
	return ret;
}

static TEE_Result sharc_verify(struct sb_hdr *hdr) {
	uint32_t hash_type;
	uint32_t length;
	uint32_t digest_size;
	uint8_t digest[TEE_SHA256_HASH_SIZE];
	void *hash_ctx;
	TEE_Result ret;

	ret = find_hash_type(hdr, &hash_type);
	if (TEE_SUCCESS != ret)
		return ret;

	digest_size = TEE_SHA256_HASH_SIZE;
	if (hash_type == TEE_ALG_SHA224)
		digest_size = TEE_SHA224_HASH_SIZE;

	ret = crypto_hash_alloc_ctx(&hash_ctx, hash_type);
	if (TEE_SUCCESS != ret) {
		DMSG("Failed to allocate crypto hash ctx\n");
		return ret;
	}

	ret = crypto_hash_init(hash_ctx);
	if (TEE_SUCCESS != ret) {
		DMSG("Failed to init crypto hash ctx\n");
		goto cleanup_hash;
	}

	ret = crypto_hash_update(hash_ctx, (uint8_t *)hdr->attributes,
		sizeof(hdr->attributes));
	if (TEE_SUCCESS != ret) {
		DMSG("Failed to calculate image hash\n");
		goto cleanup_hash;
	}

	length = TEE_U32_BSWAP(hdr->length);
	ret = crypto_hash_update(hash_ctx, (uint8_t *)(hdr+1), length);
	if (TEE_SUCCESS != ret) {
		DMSG("Failed to calculate image hash\n");
		goto cleanup_hash;
	}

	ret = crypto_hash_final(hash_ctx, digest, digest_size);
	if (TEE_SUCCESS != ret) {
		DMSG("failed to finish image hash\n");
		goto cleanup_hash;
	}

	if (memcmp(digest, hdr->hash, digest_size) != 0) {
		DMSG("Hash mismatch\n");
		ret = TEE_ERROR_SIGNATURE_INVALID;
		goto cleanup_hash;
	}

	ret = __signature_verify(ADI_OTP_ID_public_key0, hdr, digest, digest_size);
	if (TEE_SUCCESS != ret) {
		DMSG("public key 0 could not verify\n");
		ret = __signature_verify(ADI_OTP_ID_public_key1, hdr, digest, digest_size);
		if (TEE_SUCCESS != ret)
			DMSG("public key 1 could not verify\n");
	}

cleanup_hash:
	crypto_hash_free_ctx(hash_ctx);
	return ret;
}

/**
 * Calculate virtual address for a given load address
 *
 * The L1 region has several gaps but doing precise mappings creates many more
 * translation entries than we would like for OPTEE, so we expect less granular
 * mappings to cover the entire range for each core and one for all of L2
 */
static TEE_Result sharc_la_to_va(uint32_t coreid, uint32_t la, vaddr_t *va) {
	uint32_t offset;

	if (la >= state[coreid].l1_load_base
		&& la < state[coreid].l1_load_base + state[coreid].l1_size)
	{
		offset = la - state[coreid].l1_load_base;
		*va = state[coreid].l1_base + offset;
		return TEE_SUCCESS;
	}
	else if (la >= ADSP_SC5XX_L2_BASE
		&& la < ADSP_SC5XX_L2_BASE + ADSP_SC5XX_L2_SIZE)
	{
		offset = la - ADSP_SC5XX_L2_BASE;
		*va = l2_base + offset;
		return TEE_SUCCESS;
	}

	return TEE_ERROR_BAD_PARAMETERS;
}

/**
 * This checks for an ldr block header, not a signed header
 */
static TEE_Result sharc_check_hdr(struct ldr_hdr *hdr) {
	if (hdr->bcode_flag.bHdrSIGN == 0xad
		|| hdr->bcode_flag.bHdrSIGN == 0xac
		|| hdr->bcode_flag.bHdrSIGN == 0xab)
	{
		return TEE_SUCCESS;
	}

	return TEE_ERROR_BAD_FORMAT;
}

static int is_final(struct ldr_hdr *hdr) {
	return hdr->bcode_flag.bFlag_final;
}

static int is_empty(struct ldr_hdr *hdr) {
	return hdr->bcode_flag.bFlag_ignore || (hdr->byte_count == 0);
}

static int is_first(struct ldr_hdr *hdr) {
	return hdr->bcode_flag.bFlag_first;
}

static TEE_Result sharc_load(uint32_t coreid, void *data_start, size_t data_sz) {
	TEE_Result ret;
	struct ldr_hdr *block_hdr = data_start;
	uintptr_t data;
	uintptr_t data_end = (uintptr_t) data_start + data_sz;
	size_t offset;
	vaddr_t target_vaddr;
	bool done = false;

	offset = 0;
	for (data = (uintptr_t) data_start; !done && data < data_end;
		data = data + offset)
	{
		if (data + sizeof(struct ldr_hdr) > data_end) {
			EMSG("Incomplete LDR header in buffer 0x%lx + 0x%lx > 0x%lx\n",
				data, sizeof(struct ldr_hdr), data_end);
			return TEE_ERROR_SHORT_BUFFER;
		}

		block_hdr = (struct ldr_hdr *) data;
		offset = block_hdr->bcode_flag.bFlag_fill ? 0 : block_hdr->byte_count;
		offset += sizeof(struct ldr_hdr);

		DMSG("Process LDR block at %p: (0x%x, 0x%x, 0x%x, 0x%x)\n", block_hdr,
			block_hdr->bcode_int, block_hdr->target_addr, block_hdr->byte_count,
			block_hdr->argument);

		ret = sharc_check_hdr(block_hdr);
		if (TEE_SUCCESS != ret) {
			EMSG("Block header check failed\n");
			return ret;
		}

		if (is_final(block_hdr))
			done = true;

		// This is a device address and should not be translated
		if (is_first(block_hdr))
			state[coreid].load_addr = block_hdr->target_addr;

		if (is_empty(block_hdr))
			continue;

		// These need to become virtual addresses for us to write to the
		// physical address in the ldr file
		ret = sharc_la_to_va(coreid, block_hdr->target_addr, &target_vaddr);
		if (TEE_SUCCESS != ret) {
			EMSG("DA 0x%x could not be translated, probably incorrect\n",
				block_hdr->target_addr);
			return ret;
		}

		if (block_hdr->bcode_flag.bFlag_fill) {
			io_memset(target_vaddr, block_hdr->argument, block_hdr->byte_count);
		}
		else {
			if (data + offset > data_end) {
				EMSG("LDR block claims to be larger than buffer!\n");
				return TEE_ERROR_SHORT_BUFFER;
			}

			// First byte after ldr header
			io_memcpy(target_vaddr, block_hdr + 1, block_hdr->byte_count);
		}
	}

	if (!is_final(block_hdr)) {
		EMSG("LDR finished buffer but didn't find the final block!\n");
		return TEE_ERROR_SHORT_BUFFER;
	}

	return TEE_SUCCESS;
}

static TEE_Result sharc_stop(uint32_t coreid) {
	TEE_Result ret;

	adi_rcu_set_svect(coreid, ADSP_SHARC_IDLE_ADDR);

	ret = adi_rcu_stop_core(coreid, state[coreid].irq);
	if (TEE_SUCCESS != ret)
		return ret;

	ret = adi_rcu_reset_core(coreid);
	if (TEE_SUCCESS != ret)
		return ret;

	state[coreid].state = SHARC_CORE_STOPPED;
	return TEE_SUCCESS;
}

static TEE_Result sharc_start(uint32_t coreid) {
	TEE_Result ret;

	DMSG("Set SHARC %d loadaddr to 0x%lx\n", coreid, state[coreid].load_addr);
	adi_rcu_set_svect(coreid, state[coreid].load_addr);

	ret = adi_rcu_reset_core(coreid);
	if (ret)
		return ret;

	ret = adi_rcu_start_core(coreid);
	if (TEE_SUCCESS != ret)
		return ret;

	state[coreid].state = SHARC_CORE_RUNNING;
	return TEE_SUCCESS;
}

static TEE_Result adi_sharc_init(void) {
	state[0].l1_load_base = ADSP_SC5XX_SHARC0_L1_BASE;
	state[0].l1_base = core_mmu_get_va(ADSP_SC5XX_SHARC0_L1_BASE, MEM_AREA_IO_SEC,
		ADSP_SC5XX_SHARC0_L1_SIZE);
	state[0].l1_size = ADSP_SC5XX_SHARC0_L1_SIZE;

	state[1].l1_load_base = ADSP_SC5XX_SHARC1_L1_BASE;
	state[1].l1_base = core_mmu_get_va(ADSP_SC5XX_SHARC1_L1_BASE, MEM_AREA_IO_SEC,
		ADSP_SC5XX_SHARC1_L1_SIZE);
	state[1].l1_size = ADSP_SC5XX_SHARC1_L1_SIZE;

	l2_base = core_mmu_get_va(ADSP_SC5XX_L2_BASE, MEM_AREA_IO_SEC, ADSP_SC5XX_L2_SIZE);

	if (!state[0].l1_base || !state[1].l1_base || !l2_base)
		panic();

	state[0].irq = ADSP_SC5XX_SHARC0_IRQ;
	state[1].irq = ADSP_SC5XX_SHARC1_IRQ;

	return TEE_SUCCESS;
}

driver_init(adi_sharc_init);

static TEE_Result cmd_load(uint32_t param_types, TEE_Param params[TEE_NUM_PARAMS]) {
	uint32_t coreid;
	void *data;
	size_t size;
	struct sb_hdr *hdr;
	TEE_Result ret;

	const uint32_t expected = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT,
		TEE_PARAM_TYPE_MEMREF_INPUT,
		TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE);

	if (param_types != expected) {
		DMSG("sharc pta: param types mismatch\n");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	if (params[0].value.a >= ADSP_SC5XX_NUM_SHARC_CORES) {
		DMSG("Invalid coreid %d > %d\n", params[0].value.a,
			ADSP_SC5XX_NUM_SHARC_CORES-1);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	coreid = params[0].value.a;

	if (state[coreid].state != SHARC_CORE_STOPPED) {
		DMSG("Cannot load code to running core\n");
		return TEE_ERROR_BAD_STATE;
	}

	data = params[1].memref.buffer;
	size = params[1].memref.size;
	hdr = data;
	ret = check_secure_header(data, size);

	if (TEE_SUCCESS == ret) {
#ifndef CFG_ADI_SHARC_ALLOW_UNVERIFIED
		ret = sharc_verify(hdr);
		if (TEE_SUCCESS != ret)
			return ret;
#endif
		data = hdr + 1;
		size = size - sizeof(struct sb_hdr);
	}
#ifndef CFG_ADI_SHARC_ALLOW_UNVERIFIED
	else
		return ret;
#endif

	return sharc_load(coreid, data, size);
}

static TEE_Result cmd_stop(uint32_t param_types, TEE_Param params[TEE_NUM_PARAMS]) {
	uint32_t coreid;
	const uint32_t expected = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT,
		TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE);

	if (param_types != expected) {
		DMSG("sharc pta: param types mismatch\n");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	if (params[0].value.a >= ADSP_SC5XX_NUM_SHARC_CORES) {
		DMSG("Invalid coreid %d > %d\n", params[0].value.a,
			ADSP_SC5XX_NUM_SHARC_CORES-1);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	coreid = params[0].value.a;
	if (state[coreid].state != SHARC_CORE_STOPPED)
		return sharc_stop(coreid);
	return TEE_SUCCESS;
}

static TEE_Result cmd_start(uint32_t param_types, TEE_Param params[TEE_NUM_PARAMS]) {
	uint32_t coreid;
	const uint32_t expected = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT,
		TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE);

	if (param_types != expected) {
		DMSG("sharc pta: param types mismatch\n");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	if (params[0].value.a >= ADSP_SC5XX_NUM_SHARC_CORES) {
		DMSG("Invalid coreid %d > %d\n", params[0].value.a,
			ADSP_SC5XX_NUM_SHARC_CORES-1);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	coreid = params[0].value.a;
	if (state[coreid].state == SHARC_CORE_STOPPED)
		return sharc_start(coreid);
	return TEE_SUCCESS;
}

static TEE_Result cmd_verify(uint32_t param_types, TEE_Param params[TEE_NUM_PARAMS]) {
	void *data;
	size_t size;
	struct sb_hdr *hdr;
	TEE_Result ret;

	const uint32_t expected = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
		TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE);

	if (param_types != expected) {
		DMSG("sharc pta: param types mismatch\n");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	data = params[0].memref.buffer;
	size = params[0].memref.size;
	hdr = data;
	ret = check_secure_header(data, size);
	if (TEE_SUCCESS != ret)
		return ret;

	return sharc_verify(hdr);
}

static TEE_Result adi_sharc_invoke_command(void *session __unused, uint32_t cmd,
	uint32_t param_types, TEE_Param params[TEE_NUM_PARAMS])
{
	switch (cmd) {
	case ADI_SHARC_CMD_LOAD:
		return cmd_load(param_types, params);
	case ADI_SHARC_CMD_START:
		return cmd_start(param_types, params);
	case ADI_SHARC_CMD_STOP:
		return cmd_stop(param_types, params);
	case ADI_SHARC_CMD_VERIFY:
		return cmd_verify(param_types, params);
	default:
		DMSG("sharc pta: received invalid command %d\n", cmd);
		return TEE_ERROR_BAD_PARAMETERS;
	}
}

pseudo_ta_register(
	.uuid = PTA_ADI_SHARC_UUID,
	.name = PTA_NAME,
	.flags = PTA_DEFAULT_FLAGS | TA_FLAG_DEVICE_ENUM,
	.invoke_command_entry_point = adi_sharc_invoke_command
);
