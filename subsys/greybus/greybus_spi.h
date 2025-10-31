/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GREYBUS_SPI_H_
#define _GREYBUS_SPI_H_

#include <stdint.h>
#include <greybus/greybus_protocols.h>

extern const struct gb_driver gb_spi_driver;

struct gb_spi_device_data {
	struct gb_spi_device_config_response data;
	uint8_t id;
};

struct gb_spi_driver_data {
	const struct gb_spi_device_data *devices;
	const struct device *dev;
	uint8_t device_num;
};

#endif // _GREYBUS_SPI_H_
