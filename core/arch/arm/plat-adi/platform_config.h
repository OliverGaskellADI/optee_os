/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022, Analog Devices, Inc.
 */
#ifndef PLATFORM_CONFIG_H
#define PLATFORM_CONFIG_H

#include <mm/generic_ram_layout.h>

#define STACK_ALIGNMENT 64

#include <platform_flavor_config.h>

// Override default translation table configuration
#define MAX_XLAT_TABLES 12

#endif
