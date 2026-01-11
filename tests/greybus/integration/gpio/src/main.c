/*
 * Copyright (c) 2025 Ayush Singh, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "greybus/greybus_messages.h"
#include <zephyr/ztest.h>
#include <greybus/greybus.h>
#include <greybus-utils/manifest.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>

static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));

struct gb_msg_with_cport gb_transport_get_message(void);

ZTEST_SUITE(greybus_gpio_tests, NULL, NULL, NULL, NULL, NULL);

ZTEST(greybus_gpio_tests, test_cport_count)
{
	zassert_equal(GREYBUS_CPORT_COUNT, 2, "Invalid number of cports");
}

/* Returns msg after some common checks */
static struct gb_msg_with_cport get_first_non_event_checked(uint8_t type, uint16_t payload_len)
{
	struct gb_msg_with_cport msg = gb_transport_get_message();

	/* Peform some common checks */
	zassert_equal(msg.cport, 1, "Invalid cport");
	zassert(gb_message_is_success(msg.msg), "Request failed");
	zassert_equal(gb_message_type(msg.msg), type, "Invalid response type");
	zassert_equal(gb_message_payload_len(msg.msg), payload_len, "Invalid response size");

	return msg;
}

ZTEST(greybus_gpio_tests, test_line_count)
{
	struct gb_msg_with_cport resp;
	const struct gb_gpio_line_count_response *resp_data;
	struct gb_message *msg = gb_message_request_alloc(0, GB_GPIO_TYPE_LINE_COUNT, false);

	greybus_rx_handler(1, msg);
	resp = get_first_non_event_checked(GB_RESPONSE(GB_GPIO_TYPE_LINE_COUNT),
					   sizeof(*resp_data));

	resp_data = (const struct gb_gpio_line_count_response *)resp.msg->payload;
	/* The line count should be 1 less than actual number of gpios for some reason. */
	zassert_equal(resp_data->count, 31, "Invalid gpio count");

	gb_message_dealloc(resp.msg);
}

ZTEST(greybus_gpio_tests, test_get_direction)
{
	struct gb_msg_with_cport resp;
	const struct gb_gpio_get_direction_response *resp_data;
	struct gb_gpio_get_direction_request *req_data;
	struct gb_message *msg =
		gb_message_request_alloc(sizeof(*req_data), GB_GPIO_TYPE_GET_DIRECTION, false);

	req_data = (struct gb_gpio_get_direction_request *)msg->payload;
	req_data->which = 0;
	gpio_pin_configure(dev, 0, GPIO_INPUT);

	greybus_rx_handler(1, gb_message_copy(msg));
	resp = get_first_non_event_checked(GB_RESPONSE(GB_GPIO_TYPE_GET_DIRECTION),
					   sizeof(*resp_data));
	resp_data = (const struct gb_gpio_get_direction_response *)resp.msg->payload;

	zassert_equal(resp_data->direction, 1, "Pin should be input");

	gpio_pin_configure(dev, 0, GPIO_OUTPUT);
	greybus_rx_handler(1, msg);
	resp = get_first_non_event_checked(GB_RESPONSE(GB_GPIO_TYPE_GET_DIRECTION),
					   sizeof(*resp_data));
	resp_data = (const struct gb_gpio_get_direction_response *)resp.msg->payload;

	zassert_equal(resp_data->direction, 0, "Pin should be output");
}

ZTEST(greybus_gpio_tests, test_direction_in)
{
	struct gb_msg_with_cport resp;
	struct gb_gpio_direction_in_request *dir_in_data;
	struct gb_message *msg =
		gb_message_request_alloc(sizeof(*dir_in_data), GB_GPIO_TYPE_DIRECTION_IN, false);

	dir_in_data = (struct gb_gpio_direction_in_request *)msg->payload;
	dir_in_data->which = 0;

	/* Set direction to input */
	greybus_rx_handler(1, msg);
	resp = get_first_non_event_checked(GB_RESPONSE(GB_GPIO_TYPE_DIRECTION_IN), 0);
	gb_message_dealloc(resp.msg);

	zassert(gpio_pin_is_input(dev, 0), "Pin was not configured as input");
}

ZTEST(greybus_gpio_tests, test_get_value)
{
	struct gb_msg_with_cport resp;
	const struct gb_gpio_get_value_response *get_val_resp_data;
	struct gb_gpio_get_value_request *get_val_req_data;
	struct gb_message *msg =
		gb_message_request_alloc(sizeof(*get_val_req_data), GB_GPIO_TYPE_GET_VALUE, false);

	/* Configure pin as input for rest of the test */
	gpio_pin_configure(dev, 0, GPIO_INPUT);

	get_val_req_data = (struct gb_gpio_get_value_request *)msg->payload;

	get_val_req_data->which = 0;
	gpio_emul_input_set(dev, 0, 0);

	greybus_rx_handler(1, gb_message_copy(msg));
	resp = get_first_non_event_checked(GB_RESPONSE(GB_GPIO_TYPE_GET_VALUE),
					   sizeof(*get_val_resp_data));
	get_val_resp_data = (const struct gb_gpio_get_value_response *)resp.msg->payload;
	zassert_equal(get_val_resp_data->value, 0, "Should be 0");

	gb_message_dealloc(resp.msg);

	gpio_emul_input_set(dev, 0, 1);

	greybus_rx_handler(1, msg);
	resp = get_first_non_event_checked(GB_RESPONSE(GB_GPIO_TYPE_GET_VALUE),
					   sizeof(*get_val_resp_data));
	get_val_resp_data = (const struct gb_gpio_get_value_response *)resp.msg->payload;
	zassert_equal(get_val_resp_data->value, 1, "Should be 1");

	gb_message_dealloc(resp.msg);
}

ZTEST(greybus_gpio_tests, test_direction_out)
{
	struct gb_msg_with_cport resp;
	struct gb_gpio_direction_out_request *dir_out_data;
	struct gb_message *msg =
		gb_message_request_alloc(sizeof(*dir_out_data), GB_GPIO_TYPE_DIRECTION_OUT, false);

	dir_out_data = (struct gb_gpio_direction_out_request *)msg->payload;
	dir_out_data->which = 0;

	greybus_rx_handler(1, msg);
	resp = get_first_non_event_checked(GB_RESPONSE(GB_GPIO_TYPE_DIRECTION_OUT), 0);
	gb_message_dealloc(resp.msg);

	zassert(!gpio_pin_is_input(dev, 0), "Pin was not configured as output");
}

ZTEST(greybus_gpio_tests, test_set_value)
{
	struct gb_msg_with_cport resp;
	struct gb_gpio_set_value_request *set_val_data;
	struct gb_message *msg =
		gb_message_request_alloc(sizeof(*set_val_data), GB_GPIO_TYPE_SET_VALUE, false);

	/* Configure pin as output for rest of the test */
	gpio_pin_configure(dev, 0, GPIO_OUTPUT);

	set_val_data = (struct gb_gpio_set_value_request *)msg->payload;
	set_val_data->which = 0;
	set_val_data->value = 1;

	greybus_rx_handler(1, gb_message_copy(msg));
	resp = get_first_non_event_checked(GB_RESPONSE(GB_GPIO_TYPE_SET_VALUE), 0);
	gb_message_dealloc(resp.msg);

	zassert_equal(gpio_emul_output_get(dev, 0), 1, "Pin was not configured as output");

	set_val_data->value = 0;

	greybus_rx_handler(1, msg);
	resp = get_first_non_event_checked(GB_RESPONSE(GB_GPIO_TYPE_SET_VALUE), 0);
	gb_message_dealloc(resp.msg);

	zassert_equal(gpio_emul_output_get(dev, 0), 0, "Pin was not configured as output");
}

ZTEST(greybus_gpio_tests, test_invalid_pin_index)
{
	struct gb_msg_with_cport resp;
	struct gb_gpio_get_direction_request *req_data;
	struct gb_message *msg =
		gb_message_request_alloc(sizeof(*req_data), GB_GPIO_TYPE_GET_DIRECTION, false);

	req_data = (struct gb_gpio_get_direction_request *)msg->payload;
	req_data->which = 255;

	greybus_rx_handler(1, gb_message_copy(msg));
	resp = gb_transport_get_message();
	zassert_false(gb_message_is_success(resp.msg),
		      "Driver should have rejected invalid pin index 255");
	gb_message_dealloc(msg);
}
