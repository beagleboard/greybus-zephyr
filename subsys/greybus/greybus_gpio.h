/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GREYBUS_GPIO_H_
#define _GREYBUS_GPIO_H_

#include <zephyr/drivers/gpio.h>

extern const struct gb_driver gb_gpio_driver;

struct gb_gpio_driver_data {
	struct gpio_callback cb;
	const struct device *const dev;
	uint16_t cport;
	uint8_t ngpios;
};

#endif // _GREYBUS_GPIO_H_
