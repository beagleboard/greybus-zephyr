/*
 * Copyright (c) 2026 Pavithra CP BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __GREYBUS_CAMERA_H__
#define __GREYBUS_CAMERA_H__

#include <zephyr/toolchain.h>
#include <stdint.h>

#define GB_CAMERA_TYPE_PROTOCOL_VERSION  0x01
#define GB_CAMERA_TYPE_CAPABILITIES      0x02
#define GB_CAMERA_TYPE_CONFIGURE_STREAMS 0x03
#define GB_CAMERA_TYPE_CAPTURE           0x04
#define GB_CAMERA_TYPE_FLUSH             0x05
#define GB_CAMERA_CAP_FMT_JPEG           0x01

struct gb_camera_info {
	uint8_t state;
};

struct gb_camera_driver_data {
	const struct device *dev;
	struct gb_camera_info *info;
};

#endif /* __GREYBUS_CAMERA_H__ */
