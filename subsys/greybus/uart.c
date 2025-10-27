/*
 * Copyright (c) 2015 Google, Inc.
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

#include "greybus_uart.h"
#include "greybus_transport.h"
#include <zephyr/logging/log.h>
#include <greybus/greybus_protocols.h>
#include "greybus_internal.h"

LOG_MODULE_REGISTER(greybus_uart, CONFIG_GREYBUS_LOG_LEVEL);

/* Reserved buffer for rx data. */
#define MAX_RX_BUF_SIZE 64

/* The id of error in protocol operating. */
#define GB_UART_EVENT_PROTOCOL_ERROR 1
#define GB_UART_EVENT_DEVICE_ERROR   2

/**
 * @brief Protocol send data function.
 */
static void gb_uart_send_data(uint16_t cport, struct gb_message *req, const struct device *dev)
{
	size_t i;
	const struct gb_uart_send_data_request *req_data =
		(const struct gb_uart_send_data_request *)req->payload;

	for (i = 0; i < sys_le16_to_cpu(req_data->size); i++) {
		uart_poll_out(dev, req_data->data[i]);
	}

	gb_transport_message_empty_response_send(req, GB_OP_SUCCESS, cport);
}

/**
 * @brief Protocol set line coding function.
 */
static void gb_uart_set_line_coding(uint16_t cport, struct gb_message *req,
				    const struct device *dev)
{
	int ret;
	const struct gb_uart_set_line_coding_request *req_data =
		(const struct gb_uart_set_line_coding_request *)req->payload;
	struct uart_config conf = {
		.baudrate = sys_le32_to_cpu(req_data->rate),
	};

	switch (req_data->format) {
	case GB_SERIAL_1_STOP_BITS:
		conf.stop_bits = UART_CFG_STOP_BITS_1;
		break;
	case GB_SERIAL_1_5_STOP_BITS:
		conf.stop_bits = UART_CFG_STOP_BITS_1_5;
		break;
	case GB_SERIAL_2_STOP_BITS:
		conf.stop_bits = UART_CFG_STOP_BITS_2;
		break;
	default:
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}

	switch (req_data->parity) {
	case GB_SERIAL_NO_PARITY:
		conf.parity = UART_CFG_PARITY_NONE;
		break;
	case GB_SERIAL_ODD_PARITY:
		conf.parity = UART_CFG_PARITY_ODD;
		break;
	case GB_SERIAL_EVEN_PARITY:
		conf.parity = UART_CFG_PARITY_EVEN;
		break;
	case GB_SERIAL_MARK_PARITY:
		conf.parity = UART_CFG_PARITY_MARK;
		break;
	case GB_SERIAL_SPACE_PARITY:
		conf.parity = UART_CFG_PARITY_SPACE;
		break;
	default:
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}

	switch (req_data->data_bits) {
	case 5:
		conf.data_bits = UART_CFG_DATA_BITS_5;
		break;
	case 6:
		conf.data_bits = UART_CFG_DATA_BITS_6;
		break;
	case 7:
		conf.data_bits = UART_CFG_DATA_BITS_7;
		break;
	case 8:
		conf.data_bits = UART_CFG_DATA_BITS_8;
		break;
	case 9:
		conf.data_bits = UART_CFG_DATA_BITS_9;
		break;
	default:
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}

	switch (req_data->flow_control) {
	case 0:
		break;
	case 0x01:
		conf.flow_ctrl = UART_CFG_FLOW_CTRL_RTS_CTS;
	default:
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}

	ret = uart_configure(dev, &conf);
	gb_transport_message_empty_response_send(req, gb_errno_to_op_result(ret), cport);
}

/**
 * @brief Protocol set RTS & DTR line status function.
 */
static void gb_uart_set_control_line_state(uint16_t cport, struct gb_message *req,
					   const struct device *dev)
{
	int ret;
	const struct gb_uart_set_control_line_state_request *req_data =
		(const struct gb_uart_set_control_line_state_request *)req->payload;

	ret = uart_line_ctrl_set(dev, UART_LINE_CTRL_DTR, req_data->control & GB_UART_CTRL_DTR);
	if (ret < 0) {
		LOG_ERR("Failed to set ctrl dtr");
		return gb_transport_message_empty_response_send(req, gb_errno_to_op_result(ret),
								cport);
	}

	ret = uart_line_ctrl_set(dev, UART_LINE_CTRL_RTS, req_data->control & GB_UART_CTRL_RTS);
	if (ret < 0) {
		LOG_ERR("Failed to set ctrl rts");
	}
	gb_transport_message_empty_response_send(req, gb_errno_to_op_result(ret), cport);
}

/**
 * @brief Protocol send break function.
 */
static void gb_uart_send_break(uint16_t cport, struct gb_message *req, const struct device *dev)
{
	/* TODO: zephyr should provide API for this. */
	gb_transport_message_empty_response_send(req, GB_OP_SUCCESS, cport);
}

static void uart_irq_cb(const struct device *dev, void *user_data)
{
	uint16_t cport = POINTER_TO_UINT(user_data);
	struct gb_message *req;
	struct gb_uart_recv_data_request *req_data;
	int ret;

	if (!uart_irq_update(dev) && !uart_irq_rx_ready(dev)) {
		return;
	}

	req = gb_message_request_alloc(MAX_RX_BUF_SIZE, GB_UART_TYPE_RECEIVE_DATA, true);
	if (!req) {
		LOG_ERR("Failed to allocate message");
		return;
	}
	req_data = (struct gb_uart_recv_data_request *)req->payload;

	ret = uart_fifo_read(dev, req_data->data, MAX_RX_BUF_SIZE);
	if (ret < 0) {
		LOG_ERR("Failed to read from UART");
		goto free_msg;
	}

	req_data->flags = 0;
	req_data->size = ret;
	req->header.size =
		ret + sizeof(struct gb_message) + sizeof(struct gb_uart_recv_data_request);

	if (ret) {
		gb_transport_message_send(req, cport);
	}

free_msg:
	gb_message_dealloc(req);
}

/**
 * @brief Protocol initialization function.
 *
 * This function perform the protocto initialization function, such as open
 * the cooperation device driver, launch threads, create buffers etc.
 */
static int gb_uart_init(const void *priv, uint16_t cport)
{
	const struct device *dev = priv;

	uart_irq_callback_user_data_set(dev, uart_irq_cb, UINT_TO_POINTER(cport));
	uart_irq_rx_enable(dev);

	return 0;
}

/**
 * @brief Protocol exit function.
 *
 * This function can be called when protocol terminated.
 */
static void gb_uart_exit(const void *priv)
{
	const struct device *dev = priv;

	uart_irq_rx_disable(dev);
}

static void gb_uart_handler(const void *priv, struct gb_message *msg, uint16_t cport)
{
	const struct device *dev = priv;

	switch (gb_message_type(msg)) {
	case GB_UART_TYPE_SEND_DATA:
		return gb_uart_send_data(cport, msg, dev);
	case GB_UART_TYPE_SET_LINE_CODING:
		return gb_uart_set_line_coding(cport, msg, dev);
	case GB_UART_TYPE_SET_CONTROL_LINE_STATE:
		return gb_uart_set_control_line_state(cport, msg, dev);
	case GB_UART_TYPE_SEND_BREAK:
		return gb_uart_send_break(cport, msg, dev);
	case GB_UART_TYPE_FLUSH_FIFOS:
		/* Since I am not doing any internal buffering, this is not implemented. */
		return gb_transport_message_empty_response_send(msg, GB_OP_SUCCESS, cport);
	default:
		LOG_ERR("Invalid type");
		return gb_transport_message_empty_response_send(msg, GB_OP_PROTOCOL_BAD, cport);
	}
}

struct gb_driver gb_uart_driver = {
	.init = gb_uart_init,
	.exit = gb_uart_exit,
	.op_handler = gb_uart_handler,
};
