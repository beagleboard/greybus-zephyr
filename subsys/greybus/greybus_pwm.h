/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GREYBUS_PWM_H_
#define _GREYBUS_PWM_H_

#include <stdint.h>
#include <stdbool.h>

extern const struct gb_driver gb_pwm_driver;

struct gb_pwm_channel_data {
	uint32_t duty;
	uint32_t period;
	/* false for normal. true for inverted */
	bool polarity;
};

struct gb_pwm_driver_data {
	struct gb_pwm_channel_data *channel_data;
	const struct device *dev;
	uint8_t channel_num;
};

/**
 * @brief Get the number of channels supported by a PWM controller
 *
 * @param dev PWM device
 * @return Number of channels supported, or 0 on error
 */
uint8_t gb_pwm_get_channel_count(const struct device *dev);

/**
 * @brief Initialize PWM driver data with channel count
 *
 * @param data PWM driver data to initialize
 * @return 0 on success, negative error code on failure
 */
int gb_pwm_init(struct gb_pwm_driver_data *data);

#endif // _GREYBUS_PWM_H_
