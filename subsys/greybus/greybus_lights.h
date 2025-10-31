/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GREYBUS_LIGHTS_H_
#define _GREYBUS_LIGHTS_H_

#include <stdint.h>

extern const struct gb_driver gb_lights_driver;

struct gb_lights_driver_data {
	uint8_t lights_num;
	const struct device **devs;
};

#endif // _GREYBUS_LIGHTS_H_
