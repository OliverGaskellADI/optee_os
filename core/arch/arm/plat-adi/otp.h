#ifndef PLAT_ADI_OTP_H
#define PLAT_ADI_OTP_H

#include <adi_otp_pta.h>

#include <util.h>
#include <tee_api_types.h>

struct adi_otp {
	vaddr_t otp_rom_base;
	vaddr_t control_base;
};

#define ADI_OTP_ACCESS_NONSECURE 0
#define ADI_OTP_ACCESS_SECURE 1

TEE_Result adi_otp_read(struct adi_otp *otp, uint32_t id, void *buf, uint32_t *len,
	uint32_t access);
TEE_Result adi_otp_write(struct adi_otp *otp, uint32_t id, void *buf, uint32_t len,
	uint32_t access);
TEE_Result adi_otp_invalidate(struct adi_otp *otp, uint32_t id, uint32_t access);
TEE_Result adi_otp_is_written(struct adi_otp *otp, uint32_t id, uint32_t access,
	uint32_t *written);
TEE_Result adi_otp_is_valid(struct adi_otp *otp, uint32_t id, uint32_t access,
	uint32_t *valid);

struct adi_otp *adi_get_otp(void);

int adi_otp_major(void);
int adi_otp_minor(void);

#endif
