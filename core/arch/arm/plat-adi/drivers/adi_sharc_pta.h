// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright 2023, Analog Devices, Inc.
 */
#ifndef ADI_SHARC_PTA_H
#define ADI_SHARC_PTA_H

#include <stdint.h>

#define PTA_ADI_SHARC_UUID { 0x899bfa21, 0x2961, 0x443f, \
	{ 0xa3, 0x83, 0xf6, 0x24, 0x8a, 0x53, 0xad, 0xe1 } }

#define ADI_SHARC_CORE0		0
#define ADI_SHARC_CORE1		1

/*
 * ADI_SHARC_CMD_LOAD - Load LDR file to core.
 *
 * param[0] (in value) - Core ID to load
 * param[1] (in memref) - LDR data to load
 * param[2] unused
 * param[3] unused
 */
#define ADI_SHARC_CMD_LOAD			0x00

/*
 * ADI_SHARC_CMD_START - Start a core.
 *
 * param[0] (in value) - Core ID to start
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define ADI_SHARC_CMD_START			0x01

/*
 * ADI_SHARC_CMD_STOP - Stop a core.
 *
 * param[0] (in value) - Core ID to start
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define ADI_SHARC_CMD_STOP			0x02

/*
 * ADI_SHARC_CMD_VERIFY - Verify an ldr file's signature
 *
 * param[0] (in memref) - LDR data to verify
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define ADI_SHARC_CMD_VERIFY		0x03

#endif
