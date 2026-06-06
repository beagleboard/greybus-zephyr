/*
 * Copyright (c) 2026 Pavithra CP BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __GREYBUS_CAMERA_H__
#define __GREYBUS_CAMERA_H__

#include <zephyr/toolchain.h>
#include <stdint.h>

#define GB_CAMERA_CAP_FMT_JPEG           0x01

extern const struct gb_driver gb_camera_driver;

struct gb_camera_info {
	uint8_t state;
};

struct gb_camera_driver_data {
	const struct device *dev;
	struct gb_camera_info info;
};

#endif /* __GREYBUS_CAMERA_H__ */
