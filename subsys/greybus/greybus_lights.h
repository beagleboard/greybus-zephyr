/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GREYBUS_LIGHTS_H_
#define _GREYBUS_LIGHTS_H_

#include <stdint.h>
#include <zephyr/drivers/led_strip.h>

extern const struct gb_driver gb_lights_driver;

struct gb_led_strip_channel_data {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t brightness;
};

/**
 * @brief Greybus lights driver runtime data
 *
 * This structure stores runtime information for the Greybus lights driver,
 * including LED and LED strip device references and associated channel data.
 *
 * @param led_num Number of individual LED devices.
 * @param led_strip_num Number of LED strip devices.
 * @param devs Array of device pointers.
 *             The array is organized as follows:
 *             - First @p led_num entries correspond to individual LED devices.
 *             - Next @p led_strip_num entries correspond to LED strip devices.
 *             Total array size is (@p led_num + @p led_strip_num).
 * @param led_strips_data Array of LED strip channel data structures.
 *                        This array has @p led_strip_num entries, each containing
 *                        channel information for a corresponding LED strip device.
 */
struct gb_lights_driver_data {
	uint8_t led_num;
	uint8_t led_strip_num;
	const struct device **devs;
	struct gb_led_strip_channel_data **led_strips_data;
};

#endif // _GREYBUS_LIGHTS_H_
