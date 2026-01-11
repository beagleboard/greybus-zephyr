/*
 * Copyright (c) 2014-2015 Google Inc.
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
 *
 * Author: Fabien Parent <fparent@baylibre.com>
 */

#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/byteorder.h>
#include "greybus_transport.h"
#include "greybus_gpio.h"
#include <greybus/greybus_protocols.h>
#include "greybus_internal.h"

LOG_MODULE_REGISTER(greybus_gpio, CONFIG_GREYBUS_LOG_LEVEL);

static void gb_gpio_line_count(uint16_t cport, struct gb_message *req,
			       const struct gb_gpio_driver_data *data)
{
	const struct gb_gpio_line_count_response resp_data = {
		/* Need to return 1 less than number of GPIOs */
		.count = data->ngpios - 1,
	};

	gb_transport_message_response_success_send(req, &resp_data, sizeof(resp_data), cport);
}

static void gb_gpio_activate(uint16_t cport, struct gb_message *req,
			     const struct gb_gpio_driver_data *data)
{
	const struct gb_gpio_activate_request *request =
		(const struct gb_gpio_activate_request *)req->payload;

	if (gb_message_payload_len(req) < sizeof(*request)) {
		LOG_ERR("dropping short message");
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}
	if (request->which >= data->ngpios) {
		LOG_ERR("Invalid GPIO pin index: %u", request->which);
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}
	/* No "activation" in Zephyr. Maybe power mgmt in the future */
	gb_transport_message_empty_response_send(req, GB_OP_SUCCESS, cport);
}

static void gb_gpio_deactivate(uint16_t cport, struct gb_message *req,
			       const struct gb_gpio_driver_data *data)
{
	const struct gb_gpio_deactivate_request *request =
		(const struct gb_gpio_deactivate_request *)req->payload;

	if (gb_message_payload_len(req) < sizeof(*request)) {
		LOG_ERR("dropping short message");
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}
	if (request->which >= data->ngpios) {
		LOG_ERR("Invalid GPIO pin index: %u", request->which);
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}
	/* No "deactivation" in Zephyr. Maybe power mgmt in the future */

	gb_transport_message_empty_response_send(req, GB_OP_SUCCESS, cport);
}

static void gb_gpio_get_direction(uint16_t cport, struct gb_message *req,
				  const struct gb_gpio_driver_data *data)
{
	struct gb_gpio_get_direction_response resp_data;
	const struct gb_gpio_get_direction_request *request =
		(const struct gb_gpio_get_direction_request *)req->payload;

	if (gb_message_payload_len(req) < sizeof(*request)) {
		LOG_ERR("dropping short message");
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}
	if (request->which >= data->ngpios) {
		LOG_ERR("Invalid GPIO pin index: %u", request->which);
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}
	/* In Greybus 0 := output, 1 := input. */
	resp_data.direction = gpio_pin_is_input(data->dev, request->which);
	gb_transport_message_response_success_send(req, &resp_data, sizeof(resp_data), cport);
}

static void gb_gpio_direction_in(uint16_t cport, struct gb_message *req,
				 const struct gb_gpio_driver_data *data)
{
	uint8_t ret;
	const struct gb_gpio_direction_in_request *request =
		(const struct gb_gpio_direction_in_request *)req->payload;

	if (gb_message_payload_len(req) < sizeof(*request)) {
		LOG_ERR("dropping short message");
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}
	if (request->which >= data->ngpios) {
		LOG_ERR("Invalid GPIO pin index: %u", request->which);
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}
	ret = gb_errno_to_op_result(
		gpio_pin_configure(data->dev, (gpio_pin_t)request->which, GPIO_INPUT));
	return gb_transport_message_empty_response_send(req, ret, cport);
}

static void gb_gpio_direction_out(uint16_t cport, struct gb_message *req,
				  const struct gb_gpio_driver_data *data)
{
	int ret;
	const struct gb_gpio_direction_out_request *request =
		(const struct gb_gpio_direction_out_request *)req->payload;

	if (gb_message_payload_len(req) < sizeof(*request)) {
		LOG_ERR("dropping short message");
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}
	if (request->which >= data->ngpios) {
		LOG_ERR("Invalid GPIO pin index: %u", request->which);
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}
	ret = gpio_pin_configure(data->dev, request->which, GPIO_OUTPUT);
	if (ret != 0) {
		return gb_transport_message_empty_response_send(req, gb_errno_to_op_result(ret),
								cport);
	}

	ret = gb_errno_to_op_result(gpio_pin_set(data->dev, request->which, request->value));
	gb_transport_message_empty_response_send(req, ret, cport);
}

static void gb_gpio_get_value(uint16_t cport, struct gb_message *req,
			      const struct gb_gpio_driver_data *data)
{
	struct gb_gpio_get_value_response resp_data;
	const struct gb_gpio_get_value_request *request =
		(const struct gb_gpio_get_value_request *)req->payload;

	if (gb_message_payload_len(req) < sizeof(*request)) {
		LOG_ERR("dropping short message");
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}
	if (request->which >= data->ngpios) {
		LOG_ERR("Invalid GPIO pin index: %u", request->which);
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}
	resp_data.value = gpio_pin_get(data->dev, (gpio_pin_t)request->which);
	gb_transport_message_response_success_send(req, &resp_data, sizeof(resp_data), cport);
}

static void gb_gpio_set_value(uint16_t cport, struct gb_message *req,
			      const struct gb_gpio_driver_data *data)
{
	uint8_t ret;
	const struct gb_gpio_set_value_request *request =
		(const struct gb_gpio_set_value_request *)req->payload;

	if (gb_message_payload_len(req) < sizeof(*request)) {
		LOG_ERR("dropping short message");
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}
	if (request->which >= data->ngpios) {
		LOG_ERR("Invalid GPIO pin index: %u", request->which);
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}
	ret = gb_errno_to_op_result(gpio_pin_set(data->dev, request->which, request->value));
	gb_transport_message_empty_response_send(req, ret, cport);
}

static void gb_gpio_set_debounce(uint16_t cport, struct gb_message *req,
				 const struct gb_gpio_driver_data *data)
{
	uint8_t ret = GB_OP_SUCCESS;
	gpio_flags_t flags = 0;
	const struct gb_gpio_set_debounce_request *request =
		(const struct gb_gpio_set_debounce_request *)req->payload;

#ifdef DT_HAS_TI_CC13XX_CC26XX_GPIO_ENABLED
	flags = CC13XX_CC26XX_GPIO_DEBOUNCE;
#endif

	if (gb_message_payload_len(req) < sizeof(*request)) {
		LOG_ERR("dropping short message");
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}
	if (request->which >= data->ngpios) {
		LOG_ERR("Invalid GPIO pin index: %u", request->which);
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}
	if (sys_le16_to_cpu(request->usec) > 0) {
		ret = gb_errno_to_op_result(
			gpio_pin_configure(data->dev, (gpio_pin_t)request->which, flags));
	}

	gb_transport_message_empty_response_send(req, ret, cport);
}

static void gb_gpio_irq_mask(uint16_t cport, struct gb_message *req,
			     const struct gb_gpio_driver_data *data)
{
	uint8_t ret;
	const struct gb_gpio_irq_mask_request *request =
		(const struct gb_gpio_irq_mask_request *)req->payload;

	if (gb_message_payload_len(req) < sizeof(*request)) {
		LOG_ERR("dropping short message");
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}
	if (request->which >= data->ngpios) {
		LOG_ERR("Invalid GPIO pin index: %u", request->which);
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}
	ret = gb_errno_to_op_result(
		gpio_pin_interrupt_configure(data->dev, request->which, GPIO_INT_DISABLE));
	gb_transport_message_empty_response_send(req, ret, cport);
}

static void gb_gpio_irq_unmask(uint16_t cport, struct gb_message *req,
			       const struct gb_gpio_driver_data *data)
{
	uint8_t ret;
	const struct gb_gpio_irq_unmask_request *request =
		(const struct gb_gpio_irq_unmask_request *)req->payload;

	if (gb_message_payload_len(req) < sizeof(*request)) {
		LOG_ERR("dropping short message");
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}
	if (request->which >= data->ngpios) {
		LOG_ERR("Invalid GPIO pin index: %u", request->which);
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}
	ret = gb_errno_to_op_result(gpio_pin_interrupt_configure(
		data->dev, request->which, GPIO_INT_ENABLE | GPIO_INT_EDGE_RISING));
	gb_transport_message_empty_response_send(req, ret, cport);
}

static void gb_gpio_irq_type(uint16_t cport, struct gb_message *req,
			     const struct gb_gpio_driver_data *data)
{
	uint8_t ret;
	gpio_flags_t flags;
	const struct gb_gpio_irq_type_request *request =
		(const struct gb_gpio_irq_type_request *)req->payload;

	if (gb_message_payload_len(req) < sizeof(*request)) {
		LOG_ERR("dropping short message");
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}
	if (request->which >= data->ngpios) {
		LOG_ERR("Invalid GPIO pin index: %u", request->which);
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}
	switch (request->type) {
	case GB_GPIO_IRQ_TYPE_NONE:
		flags = GPIO_INT_DISABLE;
		break;
	case GB_GPIO_IRQ_TYPE_EDGE_RISING:
		flags = GPIO_INT_EDGE_RISING;
		break;
	case GB_GPIO_IRQ_TYPE_EDGE_FALLING:
		flags = GPIO_INT_EDGE_FALLING;
		break;
	case GB_GPIO_IRQ_TYPE_EDGE_BOTH:
		flags = GPIO_INT_EDGE_BOTH;
		break;
	case GB_GPIO_IRQ_TYPE_LEVEL_HIGH:
		flags = GPIO_INT_LEVEL_HIGH;
		break;
	case GB_GPIO_IRQ_TYPE_LEVEL_LOW:
		flags = GPIO_INT_LEVEL_LOW;
		break;
	default:
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}

	ret = gb_errno_to_op_result(gpio_pin_interrupt_configure(data->dev, request->which, flags));

	gb_transport_message_empty_response_send(req, ret, cport);
}

static void gb_gpio_handler(const void *priv, struct gb_message *msg, uint16_t cport)
{
	const struct gb_gpio_driver_data *data = priv;

	switch (gb_message_type(msg)) {
	case GB_GPIO_TYPE_LINE_COUNT:
		return gb_gpio_line_count(cport, msg, data);
	case GB_GPIO_TYPE_ACTIVATE:
		return gb_gpio_activate(cport, msg, data);
	case GB_GPIO_TYPE_DEACTIVATE:
		return gb_gpio_deactivate(cport, msg, data);
	case GB_GPIO_TYPE_GET_DIRECTION:
		return gb_gpio_get_direction(cport, msg, data);
	case GB_GPIO_TYPE_DIRECTION_IN:
		return gb_gpio_direction_in(cport, msg, data);
	case GB_GPIO_TYPE_DIRECTION_OUT:
		return gb_gpio_direction_out(cport, msg, data);
	case GB_GPIO_TYPE_GET_VALUE:
		return gb_gpio_get_value(cport, msg, data);
	case GB_GPIO_TYPE_SET_VALUE:
		return gb_gpio_set_value(cport, msg, data);
	case GB_GPIO_TYPE_SET_DEBOUNCE:
		return gb_gpio_set_debounce(cport, msg, data);
	case GB_GPIO_TYPE_IRQ_TYPE:
		return gb_gpio_irq_type(cport, msg, data);
	case GB_GPIO_TYPE_IRQ_MASK:
		return gb_gpio_irq_mask(cport, msg, data);
	case GB_GPIO_TYPE_IRQ_UNMASK:
		return gb_gpio_irq_unmask(cport, msg, data);
	default:
		LOG_ERR("Invalid type");
		gb_transport_message_empty_response_send(msg, GB_OP_INVALID, cport);
	}
}

struct gpio_irq_event_request_msg {
	struct gb_operation_msg_hdr hdr;
	struct gb_gpio_irq_event_request body;
} __packed;

static void gpio_callback_handler(const struct device *port, struct gpio_callback *cb,
				  gpio_port_pins_t pins)
{
	int ret;
	size_t i;
	struct gb_gpio_driver_data *data = CONTAINER_OF(cb, struct gb_gpio_driver_data, cb);
	uint8_t buf[sizeof(struct gpio_irq_event_request_msg)] = {0};
	struct gpio_irq_event_request_msg *msg = (struct gpio_irq_event_request_msg *)buf;

	msg->hdr.size = sys_cpu_to_le16(sizeof(buf));
	msg->hdr.type = GB_GPIO_TYPE_IRQ_EVENT;

	for (i = 0; i < GPIO_MAX_PINS_PER_PORT && pins != 0; ++i, pins >>= 1) {
		if (pins & 1) {
			msg->body.which = i;
			ret = gb_transport_message_send((const struct gb_message *)buf,
							data->cport);
			if (ret < 0) {
				LOG_ERR("GPIO irq send failed: %d", ret);
			}
		}
	}
}

static void gb_gpio_connected(const void *priv, uint16_t cport)
{
	int ret;
	struct gb_gpio_driver_data *data = (struct gb_gpio_driver_data *)priv;
	const struct gpio_driver_config *cfg = (const struct gpio_driver_config *)data->dev->config;

	data->cport = cport;
	gpio_init_callback(&data->cb, gpio_callback_handler, cfg->port_pin_mask);

	ret = gpio_add_callback(data->dev, &data->cb);
	if (ret < 0) {
		LOG_ERR("Failed to add gpio callback");
	}
}

static void gb_gpio_disconnected(const void *priv)
{
	struct gb_gpio_driver_data *data = (struct gb_gpio_driver_data *)priv;

	gpio_remove_callback(data->dev, &data->cb);
}

const struct gb_driver gb_gpio_driver = {
	.connected = gb_gpio_connected,
	.disconnected = gb_gpio_disconnected,
	.op_handler = gb_gpio_handler,
};
