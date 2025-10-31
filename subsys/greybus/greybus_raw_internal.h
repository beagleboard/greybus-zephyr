/*
 * Copyright (c) 2025 Ayush Singh, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GREYBUS_RAW_INTERNAL_H_
#define _GREYBUS_RAW_INTERNAL_H_

#include <greybus/greybus_raw.h>

extern const struct gb_driver gb_raw_driver;

struct gb_raw_driver_data {
	greybus_raw_cb_t cb;
	void *cb_priv;
};

#endif // _GREYBUS_RAW_INTERNAL_H_
