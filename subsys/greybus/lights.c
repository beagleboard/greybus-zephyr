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
#include <zephyr/drivers/led_strip.h>
#include <greybus/greybus_protocols.h>
#include "greybus_internal.h"
#include <zephyr/dt-bindings/led/led.h>
#include "greybus_heap.h"

LOG_MODULE_REGISTER(greybus_lights, CONFIG_GREYBUS_LOG_LEVEL);

#define LED_STRIP_MAX_BRIGHTNESS UINT8_MAX
#define RGB_R_MASK               GENMASK(23, 16)
#define RGB_G_MASK               GENMASK(15, 8)
#define RGB_B_MASK               GENMASK(7, 0)

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
	const struct gb_lights_get_lights_response resp_data = {
		.lights_count = data->led_num + data->led_strip_num,
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
	struct gb_lights_get_light_config_response resp_data;
	const struct device *const dev = data->devs[req_data->id];
	const struct led_info *info;
	int ret;

	if (req_data->id < data->led_num) {
		/* LED Device */
		ret = led_get_info(dev, req_data->id, &info);
		/* Device name always needs to be set */
		if (ret < 0) {
			strncpy(resp_data.name, dev->name, sizeof(resp_data.name));
		} else {
			strncpy(resp_data.name, info->label, sizeof(resp_data.name));
		}
		resp_data.channel_count = 1;

	} else {
		/* LED Strip device */
		strncpy(resp_data.name, dev->name, sizeof(resp_data.name));
		resp_data.channel_count = led_strip_length(dev);
	}

	return gb_transport_message_response_success_send(req, &resp_data, sizeof(resp_data),
							  cport);
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
	const struct led_driver_api *api;
	const struct gb_lights_get_channel_config_request *req_data =
		(const struct gb_lights_get_channel_config_request *)req->payload;
	const struct device *dev = data->devs[req_data->light_id];
	struct gb_lights_get_channel_config_response resp_data = {
		.flags = 0,
		.mode = 0,
		.color = 0,
	};

	if (req_data->light_id < data->led_num) {
		api = DEVICE_API_GET(led, dev);
		if (api->blink) {
			resp_data.flags |= GB_LIGHT_CHANNEL_BLINK;
		}
		resp_data.max_brightness = LED_BRIGHTNESS_MAX;
	} else {
		resp_data.max_brightness = LED_STRIP_MAX_BRIGHTNESS;
		resp_data.flags |= GB_LIGHT_CHANNEL_MULTICOLOR;
		/* Represents RGB led in linux */
		resp_data.color = LED_COLOR_ID_RED + LED_COLOR_ID_BLUE + LED_COLOR_ID_GREEN;
		strncpy(resp_data.color_name, "RGB", sizeof(resp_data.color_name));
	}

	gb_transport_message_response_success_send(req, &resp_data, sizeof(resp_data), cport);
}

static int gb_lights_led_strip_update(const struct device *dev,
				      const struct gb_led_strip_channel_data data[])
{
	size_t len = led_strip_length(dev);
	struct led_rgb *scratch;
	int ret;

	scratch = gb_alloc(sizeof(struct led_rgb *) * len);
	if (!scratch) {
		return -ENOMEM;
	}

	for (size_t i = 0; i < len; ++i) {
		scratch[i].r = (data[i].r * data[i].brightness) / LED_STRIP_MAX_BRIGHTNESS;
		scratch[i].g = (data[i].g * data[i].brightness) / LED_STRIP_MAX_BRIGHTNESS;
		scratch[i].b = (data[i].b * data[i].brightness) / LED_STRIP_MAX_BRIGHTNESS;
	}

	ret = led_strip_update_rgb(dev, scratch, len);

	gb_free(scratch);

	return ret;
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
	const struct device *dev = data->devs[req_data->light_id];
	struct gb_led_strip_channel_data *led_strip_data;
	int ret;

	if (req_data->light_id < data->led_num) {
		ret = gb_errno_to_op_result(
			led_set_brightness(dev, req_data->light_id, req_data->brightness));
	} else {
		led_strip_data = data->led_strips_data[req_data->light_id - data->led_num];

		led_strip_data[req_data->channel_id].brightness = req_data->brightness;

		ret = gb_errno_to_op_result(gb_lights_led_strip_update(dev, led_strip_data));
	}

	gb_transport_message_empty_response_send(req, ret, cport);
}

static void gb_lights_set_blink(uint16_t cport, struct gb_message *req,
				const struct gb_lights_driver_data *data)
{
	int ret;
	const struct gb_lights_blink_request *req_data =
		(const struct gb_lights_blink_request *)req->payload;

	if (req_data->light_id < data->led_num) {
		ret = gb_errno_to_op_result(led_blink(data->devs[req_data->light_id],
						      req_data->light_id, req_data->time_on_ms,
						      req_data->time_off_ms));
	} else {
		/* LED Strip does not support blink */
		ret = GB_OP_INVALID;
	}

	gb_transport_message_empty_response_send(req, ret, cport);
}

static void gb_lights_set_color(uint16_t cport, struct gb_message *req,
				const struct gb_lights_driver_data *data)
{
	const struct gb_lights_set_color_request *req_data =
		(const struct gb_lights_set_color_request *)req->payload;
	const struct device *dev = data->devs[req_data->light_id];
	struct gb_led_strip_channel_data *led_strip_data;
	int ret;

	if (req_data->light_id < data->led_num) {
		ret = GB_OP_INVALID;
	} else {
		led_strip_data = data->led_strips_data[req_data->light_id - data->led_num];

		/* Assume color is in 0x00RRGGBB format */
		led_strip_data[req_data->channel_id].r = FIELD_GET(RGB_R_MASK, req_data->color);
		led_strip_data[req_data->channel_id].g = FIELD_GET(RGB_G_MASK, req_data->color);
		led_strip_data[req_data->channel_id].b = FIELD_GET(RGB_B_MASK, req_data->color);

		ret = gb_errno_to_op_result(gb_lights_led_strip_update(dev, led_strip_data));
	}

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
		return gb_lights_set_blink(cport, msg, data);
	case GB_LIGHTS_TYPE_SET_COLOR:
		return gb_lights_set_color(cport, msg, data);
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
