/**
 * Copyright (c) 2015 Google, Inc.
 * Copyright (c) 2025 Ayush Singh, BeagleBoard.org
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "greybus_transport.h"
#include "greybus_pwm.h"
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>
#include <zephyr/errno.h>
#include <greybus/greybus_protocols.h>
#include "greybus_internal.h"

LOG_MODULE_REGISTER(greybus_pwm, CONFIG_GREYBUS_LOG_LEVEL);

/**
 * @brief Get the number of channels supported by a PWM controller
 *
 * This function attempts to determine the number of channels by probing
 * the PWM device. It tries channels starting from 0 until it encounters
 * an error, indicating that channel doesn't exist.
 *
 * @param dev PWM device
 * @return Number of channels supported, or 0 on error
 */
uint8_t gb_pwm_get_channel_count(const struct device *dev)
{
	uint8_t channel = 0;
	int ret;

	if (dev == NULL) {
		return 0;
	}

	/* Probe channels by trying to get cycles per second for each channel.
	 * When we get an error, we've reached the maximum channel count.
	 * Most PWM controllers support at least 4 channels, so we'll probe
	 * up to a reasonable maximum (e.g., 16 channels).
	 */
	for (channel = 0; channel < 16; channel++) {
		uint32_t cycles_per_sec = 0;
		ret = pwm_get_cycles_per_sec(dev, channel, &cycles_per_sec);
		if (ret != 0) {
			/* This channel doesn't exist, so we've found the count */
			break;
		}
	}

	return channel;
}

static void gb_pwm_protocol_count(uint16_t cport, struct gb_message *req,
				  struct gb_pwm_driver_data *data)
{
	/* The spec states that count should be 1 less than the number of channels. */
	const struct gb_pwm_count_response resp_data = {
		.count = data->channel_num - 1,
	};

	gb_transport_message_response_success_send(req, &resp_data, sizeof(resp_data), cport);
}

/**
 * @brief Configure specific generator for a particular duty cycle and period.
 */
static void gb_pwm_protocol_config(uint16_t cport, struct gb_message *req,
				   struct gb_pwm_driver_data *data)
{
	const struct gb_pwm_config_request *req_data =
		(const struct gb_pwm_config_request *)req->payload;

	if (req_data->which >= data->channel_num) {
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}

	data->channel_data[req_data->which].duty = sys_le32_to_cpu(req_data->duty);
	data->channel_data[req_data->which].period = sys_le32_to_cpu(req_data->period);

	gb_transport_message_empty_response_send(req, GB_OP_SUCCESS, cport);
}

/**
 * @brief Configure specific generator for a particular polarity.
 */
static void gb_pwm_protocol_polarity(uint16_t cport, struct gb_message *req,
				     struct gb_pwm_driver_data *data)
{
	const struct gb_pwm_polarity_request *req_data =
		(const struct gb_pwm_polarity_request *)req->payload;

	if (req_data->which >= data->channel_num) {
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}

	data->channel_data[req_data->which].polarity = (req_data->polarity == 1);

	gb_transport_message_empty_response_send(req, GB_OP_SUCCESS, cport);
}

/**
 * @brief Enable a specific generator to start toggling.
 */
static void gb_pwm_protocol_enable(uint16_t cport, struct gb_message *req,
				   struct gb_pwm_driver_data *data)
{
	const struct gb_pwm_enable_request *req_data =
		(const struct gb_pwm_enable_request *)req->payload;
	const struct gb_pwm_channel_data *chan;
	int ret;

	if (req_data->which >= data->channel_num) {
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}

	chan = &data->channel_data[req_data->which];

	ret = pwm_set(data->dev, req_data->which, chan->period, chan->duty,
		      (chan->polarity) ? PWM_POLARITY_INVERTED : PWM_POLARITY_NORMAL);
	gb_transport_message_empty_response_send(req, gb_errno_to_op_result(ret), cport);
}

/**
 * @brief Stop the pulse on a specific channel.
 */
static void gb_pwm_protocol_disable(uint16_t cport, struct gb_message *req,
				    struct gb_pwm_driver_data *data)
{
	const struct gb_pwm_disable_request *req_data =
		(const struct gb_pwm_disable_request *)req->payload;
	const struct gb_pwm_channel_data *chan;
	int ret;

	if (req_data->which >= data->channel_num) {
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}

	chan = &data->channel_data[req_data->which];

	ret = pwm_set(data->dev, req_data->which, chan->period, 0, 0);
	gb_transport_message_empty_response_send(req, gb_errno_to_op_result(ret), cport);
}

/*
 * This structure is to define each PWM protocol operation of handling function.
 */
static void gb_pwm_handler(const void *priv, struct gb_message *msg, uint16_t cport)
{
	struct gb_pwm_driver_data *data = (struct gb_pwm_driver_data *)priv;

	switch (gb_message_type(msg)) {
	case GB_PWM_TYPE_PWM_COUNT:
		return gb_pwm_protocol_count(cport, msg, data);
	/* No activate/deactivate for PWM. Maybe can do pm stuff at some point. */
	case GB_PWM_TYPE_ACTIVATE:
	case GB_PWM_TYPE_DEACTIVATE:
		return gb_transport_message_empty_response_send(msg, GB_OP_SUCCESS, cport);
	case GB_PWM_TYPE_CONFIG:
		return gb_pwm_protocol_config(cport, msg, data);
	case GB_PWM_TYPE_POLARITY:
		return gb_pwm_protocol_polarity(cport, msg, data);
	case GB_PWM_TYPE_ENABLE:
		return gb_pwm_protocol_enable(cport, msg, data);
	case GB_PWM_TYPE_DISABLE:
		return gb_pwm_protocol_disable(cport, msg, data);
	default:
		LOG_ERR("Invalid type");
		return gb_transport_message_empty_response_send(msg, GB_OP_PROTOCOL_BAD, cport);
	}
}

/**
 * @brief Initialize PWM driver data with channel count
 *
 * This function initializes the channel_num field if it wasn't set from
 * device tree (i.e., if it's 0). It queries the PWM device to determine
 * the actual number of channels.
 *
 * @param data PWM driver data to initialize
 * @return 0 on success, negative error code on failure
 */
int gb_pwm_init(struct gb_pwm_driver_data *data)
{
	uint8_t detected_channels;
	uint8_t max_channels = 16; /* Default allocation when channels not in DT */

	if (data == NULL || data->dev == NULL) {
		return -EINVAL;
	}

	/* If channel_num is 0, it means it wasn't set from device tree,
	 * so we need to query it at runtime.
	 */
	if (data->channel_num == 0) {
		detected_channels = gb_pwm_get_channel_count(data->dev);
		if (detected_channels == 0) {
			LOG_ERR("Failed to determine PWM channel count for device %s",
				data->dev->name);
			return -EINVAL;
		}

		/* Clamp to maximum allocated size (16 when channels not in DT) */
		if (detected_channels > max_channels) {
			LOG_WRN("PWM device %s has %u channels, but only %u allocated. "
				"Consider adding 'channels' property to device tree.",
				data->dev->name, detected_channels, max_channels);
			data->channel_num = max_channels;
		} else {
			data->channel_num = detected_channels;
		}

		LOG_DBG("PWM device %s has %u channels", data->dev->name, data->channel_num);
	}

	return 0;
}

const struct gb_driver gb_pwm_driver = {
	.op_handler = gb_pwm_handler,
};
