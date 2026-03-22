/*
 * Copyright (c) 2026 Ayush Singh, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "greybus/greybus_protocols.h"
#include <zephyr/ztest.h>
#include <greybus/greybus.h>
#include <greybus-utils/manifest.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/drivers/led.h>

struct gb_msg_with_cport gb_transport_get_message(void);

ZTEST_SUITE(greybus_led_tests, NULL, NULL, NULL, NULL, NULL);

ZTEST(greybus_led_tests, test_cport_count)
{
	zassert_equal(GREYBUS_CPORT_COUNT, 2, "Invalid number of cports");
}

ZTEST(greybus_led_tests, test_get_lights)
{
	struct gb_msg_with_cport resp;
	const struct gb_lights_get_lights_response *resp_data;
	struct gb_message *msg = gb_message_request_alloc(0, GB_LIGHTS_TYPE_GET_LIGHTS, false);

	greybus_rx_handler(1, msg);

	resp = gb_transport_get_message();

	zassert(gb_message_is_success(resp.msg), "Request failed");
	zassert_equal(gb_message_type(resp.msg), GB_RESPONSE(GB_LIGHTS_TYPE_GET_LIGHTS),
		      "Invalid response type");
	zassert_equal(gb_message_payload_len(resp.msg), sizeof(*resp_data),
		      "Invalid response data");

	resp_data = (const struct gb_lights_get_lights_response *)resp.msg->payload;

	zassert_equal(resp_data->lights_count, 1, "Expected only 1 led");

	gb_message_dealloc(resp.msg);
}

ZTEST(greybus_led_tests, test_get_lights_config)
{
	struct gb_msg_with_cport resp;
	const struct gb_lights_get_light_config_response *resp_data;
	const struct gb_lights_get_light_config_request req_data = {.id = 0};
	struct gb_message *msg = gb_message_request_alloc_with_payload(
		&req_data, sizeof(req_data), GB_LIGHTS_TYPE_GET_LIGHT_CONFIG, false);

	greybus_rx_handler(1, msg);

	resp = gb_transport_get_message();

	zassert(gb_message_is_success(resp.msg), "Request failed");
	zassert_equal(gb_message_type(resp.msg), GB_RESPONSE(GB_LIGHTS_TYPE_GET_LIGHT_CONFIG),
		      "Invalid response type");
	zassert_equal(gb_message_payload_len(resp.msg), sizeof(*resp_data),
		      "Invalid response data");

	resp_data = (const struct gb_lights_get_light_config_response *)resp.msg->payload;

	zassert_equal(resp_data->channel_count, 1, "Expected only 1 channel");

	gb_message_dealloc(resp.msg);
}

ZTEST(greybus_led_tests, test_get_channel_config)
{
	struct gb_msg_with_cport resp;
	const struct gb_lights_get_channel_config_response *resp_data;
	const struct gb_lights_get_channel_config_request req_data = {
		.light_id = 0,
		.channel_id = 0,
	};
	struct gb_message *msg = gb_message_request_alloc_with_payload(
		&req_data, sizeof(req_data), GB_LIGHTS_TYPE_GET_CHANNEL_CONFIG, false);

	greybus_rx_handler(1, msg);

	resp = gb_transport_get_message();

	zassert(gb_message_is_success(resp.msg), "Request failed");
	zassert_equal(gb_message_type(resp.msg), GB_RESPONSE(GB_LIGHTS_TYPE_GET_CHANNEL_CONFIG),
		      "Invalid response type");
	zassert_equal(gb_message_payload_len(resp.msg), sizeof(*resp_data),
		      "Invalid response data");

	resp_data = (const struct gb_lights_get_channel_config_response *)resp.msg->payload;

	zassert_equal(resp_data->mode, 0, "Mode should be 0 since it is not implemented yet");
	zassert_equal(resp_data->color, 0, "Color should be 0 since it is not implemented yet");
	/* TODO: Make changes to zephyr subsystem to have actual brightness in led subsystem */
	zassert_equal(resp_data->max_brightness, LED_BRIGHTNESS_MAX,
		      "LED brightness in zephyr is percentage, not raw numbers.");
	zassert_equal(resp_data->flags, 0,
		      "Flags should be 0 since generic blink is not implemented in zephyr yet.");

	gb_message_dealloc(resp.msg);
}
