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
#include "greybus_lights.h"
#include <zephyr/logging/log.h>
#include <zephyr/drivers/led.h>
#include <greybus/greybus_protocols.h>
#include "greybus_internal.h"

LOG_MODULE_REGISTER(greybus_lights, CONFIG_GREYBUS_LOG_LEVEL);

/**
 * @brief Returns lights count of lights driver
 *
 * This operation allows the AP Module to get the number of how many lights
 * are supported in the lights device driver
 *
 * @param operation pointer to structure of Greybus operation message
 * @return GB_OP_SUCCESS on success, error code on failure
 */
static void gb_lights_get_lights(uint16_t cport, struct gb_message *req,
				 const struct gb_lights_driver_data *data)
{
	/* TODO: Add API in zephyr to get led count */
	const struct gb_lights_get_lights_response resp_data = {
		.lights_count = data->lights_num,
	};

	gb_transport_message_response_success_send(req, &resp_data, sizeof(resp_data), cport);
}

/**
 * @brief Returns light configuration of specific light
 *
 * This operation allows the AP Module to get the light configuration of
 * specific light ID. The caller will get both light name and channel number
 * of this light from the lights device driver
 *
 * @param operation pointer to structure of Greybus operation message
 * @return GB_OP_SUCCESS on success, error code on failure
 */
static void gb_lights_get_light_config(uint16_t cport, struct gb_message *req,
				       const struct gb_lights_driver_data *data)
{
	const struct gb_lights_get_light_config_request *req_data =
		(const struct gb_lights_get_light_config_request *)req->payload;
	struct gb_lights_get_light_config_response resp_data = {
		.channel_count = 1,
	};
	const struct device *const dev = data->devs[req_data->id];
	const struct led_info *info;
	int ret;

	/* This function does not seem to be required impl. So in case of failure, just assume that
	 * channel count is 1 */
	ret = led_get_info(dev, req_data->id, &info);
	/* Device name always needs to be set */
	if (ret >= 0) {
		strncpy(resp_data.name, info->label, sizeof(resp_data.name));
	} else {
		strncpy(resp_data.name, dev->name, sizeof(resp_data.name));
	}

	gb_transport_message_response_success_send(req, &resp_data, sizeof(resp_data), cport);
}

/**
 * @brief Returns channel configuration of specific channel
 *
 * This operation allows the AP Module to get the channel configuration of
 * specific channel ID. The caller will get the configuration of this channel
 * from the lights device driver, includes channel name, modes, flags, and
 * attributes
 *
 * @param operation pointer to structure of Greybus operation message
 * @return GB_OP_SUCCESS on success, error code on failure
 */
static void gb_lights_get_channel_config(uint16_t cport, struct gb_message *req,
					 const struct gb_lights_driver_data *data)
{
	/* TODO: Implement properly */
	const struct gb_lights_get_channel_config_response resp_data = {
		.max_brightness = LED_BRIGHTNESS_MAX,
		.flags = 0,
		.mode = 0,
		.color = 0,
	};

	gb_transport_message_response_success_send(req, &resp_data, sizeof(resp_data), cport);
}

/**
 * @brief Set brightness to specific channel
 *
 * This operation allows the AP Module to determine the actual level of
 * brightness with the specified value in the lights device driver, which is
 * for the specific channel ID
 *
 * @param operation pointer to structure of Greybus operation message
 * @return GB_OP_SUCCESS on success, error code on failure
 */
static void gb_lights_set_brightness(uint16_t cport, struct gb_message *req,
				     const struct gb_lights_driver_data *data)
{
	const struct gb_lights_set_brightness_request *req_data =
		(const struct gb_lights_set_brightness_request *)req->payload;
	int ret;

	ret = gb_errno_to_op_result(led_set_brightness(data->devs[req_data->light_id],
						       req_data->light_id, req_data->brightness));
	gb_transport_message_empty_response_send(req, ret, cport);
}

/**
 * @brief Greybus Lights Protocol operation handler
 */
static void gb_lights_handler(const void *priv, struct gb_message *msg, uint16_t cport)
{
	const struct gb_lights_driver_data *data = priv;

	switch (gb_message_type(msg)) {
	case GB_LIGHTS_TYPE_GET_LIGHTS:
		return gb_lights_get_lights(cport, msg, data);
	case GB_LIGHTS_TYPE_GET_LIGHT_CONFIG:
		return gb_lights_get_light_config(cport, msg, data);
	case GB_LIGHTS_TYPE_GET_CHANNEL_CONFIG:
		return gb_lights_get_channel_config(cport, msg, data);
	case GB_LIGHTS_TYPE_SET_BRIGHTNESS:
		return gb_lights_set_brightness(cport, msg, data);
	case GB_LIGHTS_TYPE_SET_BLINK:
	case GB_LIGHTS_TYPE_SET_COLOR:
	case GB_LIGHTS_TYPE_SET_FADE:
	case GB_LIGHTS_TYPE_GET_CHANNEL_FLASH_CONFIG:
	case GB_LIGHTS_TYPE_SET_FLASH_INTENSITY:
	case GB_LIGHTS_TYPE_SET_FLASH_STROBE:
	case GB_LIGHTS_TYPE_SET_FLASH_TIMEOUT:
	case GB_LIGHTS_TYPE_GET_FLASH_FAULT:
		return gb_transport_message_empty_response_send(msg, GB_OP_INTERNAL, cport);
	default:
		LOG_ERR("Invalid type");
		return gb_transport_message_empty_response_send(msg, GB_OP_PROTOCOL_BAD, cport);
	}
}

const struct gb_driver gb_lights_driver = {
	.op_handler = gb_lights_handler,
};
