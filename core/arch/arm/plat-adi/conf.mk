PLATFORM_FLAVOR ?= adsp_sc598

ifeq ($(PLATFORM_FLAVOR), adsp_sc598)
$(call force,CFG_ARM64_core,y)
include core/arch/arm/cpu/cortex-armv8-0.mk
supported-ta-targets = ta_arm64
$(call force,CFG_GIC,y)
$(call force,CFG_ARM_GICV3,y)
$(call force,CFG_WITH_LPAE,y)
$(call force,CFG_WITH_ARM_TRUSTED_FW,y)
$(call force,CFG_SECURE_TIME_SOURCE_CNTPCT,y)

# The default memory layout for SC598:
# 0x80000000 - 0x9edfffff - Insecure DDR, split between SHARC/Linux
# 0x9ee00000 - 0x9eefffff - Insecure optee shmem for communicating with linux
# 0x9ef00000 - 0x9fefffff - Secured optee os/ta memory region
# 0x9ff00000 - 0x9fffffff - Secured BL31 memory region
# The last two regions are secured by the SMPU
CFG_TFAMEM_START ?= 0x9ff00000
CFG_TFAMEM_SIZE  ?= 0x00100000
CFG_TZDRAM_START ?= 0x9ef00000
CFG_TZDRAM_SIZE  ?= 0x01000000
CFG_SHMEM_SIZE ?= 0x00f00000
CFG_SHMEM_START ?= ($(CFG_TZDRAM_START) - $(CFG_SHMEM_SIZE))
CFG_TEE_RAM_VA_SIZE ?= 0x00400000
CFG_MMAP_REGIONS ?= 32

# OTP Configuration options. If _ALL = y then the OTP pTA makes requests as a secure
# source. Otherwise, it makes them as a nonsecure source. You may wish to set LOCK_ALL
# to y to allow userspace to lock the SoC for example, or INVALIDATE_ALL to allow
# userspace to invalidate "secure" type keys (ex. secure jtag keys) in addition to
# nonsecure type keys (ex. secure boot public keys, which are not a secret)
$(call force,CFG_ADSP_SC5XX_OTP,y)
CFG_ADI_OTP_READ_ALL ?= n
CFG_ADI_OTP_WRITE_ALL ?= n
CFG_ADI_OTP_LOCK_ALL ?= n
CFG_ADI_OTP_INVALIDATE_ALL ?= n
CFG_ADI_OTP_IS_VALID_ALL ?= y
CFG_ADI_OTP_IS_WRITTEN_ALL ?= y

else
$(error Unsupported PLATFORM_FLAVOR "$(PLATFORM_FLAVOR)")
endif

$(call force,CFG_CORE_ASLR,n)

CFG_TEE_CORE_NB_CORE ?= 1

# Enable SC5xx TRNG to replace Fortuna software PRNG
$(call force,CFG_WITH_SOFTWARE_PRNG,n)
CFG_ADSP_SC5XX_TRNG := y
$(call force,CFG_HWRNG_PTA,y)
CFG_HWRNG_QUALITY := 1024

# SHARC loader options and configuration
CFG_ADI_SHARC_LOADER ?= y
CFG_ADI_SHARC_ALLOW_UNVERIFIED ?= n

# Restrict TRU to OPTEE and provide SMC to trigger only certain masters
# If yes the TRU cannot be configured from Linux and the adi,optee-tru
# driver must be used instead
CFG_ADI_TRU ?= y
