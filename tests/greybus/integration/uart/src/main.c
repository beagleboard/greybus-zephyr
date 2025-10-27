/*
 * Copyright (c) 2025 Ayush Singh, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "greybus/greybus_messages.h"
#include "greybus/greybus_protocols.h"
#include <zephyr/ztest.h>
#include <greybus/greybus.h>
#include <greybus-utils/manifest.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/serial/uart_emul.h>

#define TX_DATA_SIZE 128

static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(euart0));

struct gb_msg_with_cport gb_transport_get_message(void);

static void *cport_connected(void)
{
	struct gb_msg_with_cport resp;
	struct gb_control_connected_request *req_data;
	struct gb_message *msg =
		gb_message_request_alloc(sizeof(*req_data), GB_CONTROL_TYPE_CONNECTED, false);

	req_data = (struct gb_control_connected_request *)msg->payload;
	req_data->cport_id = sys_cpu_to_le16(1);

	greybus_rx_handler(0, msg);
	resp = gb_transport_get_message();
	zassert_equal(resp.cport, 0, "Invalid Cport");
	zassert(gb_message_is_success(resp.msg), "Connected request failed");
	zassert_equal(gb_message_payload_len(resp.msg), 0, "Invalid response");

	gb_message_dealloc(resp.msg);

	return NULL;
}

ZTEST_SUITE(greybus_uart_tests, NULL, cport_connected, NULL, NULL, NULL);

ZTEST(greybus_uart_tests, test_cport_count)
{
	zassert_equal(GREYBUS_CPORT_COUNT, 2, "Invalid number of cports");
}

ZTEST(greybus_uart_tests, test_send_data)
{
	int i, ret;
	uint8_t buf[TX_DATA_SIZE];
	struct gb_msg_with_cport resp;
	struct gb_uart_send_data_request *req_data;
	const struct gb_uart_receive_credits_request *req_credits_data;
	struct gb_message *req = gb_message_request_alloc(sizeof(*req_data) + TX_DATA_SIZE,
							  GB_UART_TYPE_SEND_DATA, false);

	req_data = (struct gb_uart_send_data_request *)req->payload;
	req_data->size = sys_cpu_to_le16(TX_DATA_SIZE);
	for (i = 0; i < TX_DATA_SIZE; i++) {
		req_data->data[i] = i;
	}

	greybus_rx_handler(1, req);
	resp = gb_transport_get_message();

	zassert_equal(resp.cport, 1, "Invalid cport");
	zassert(gb_message_is_success(resp.msg), "Request failed");
	zassert_equal(gb_message_type(resp.msg), GB_RESPONSE(GB_UART_TYPE_SEND_DATA),
		      "Invalid response type");
	zassert_equal(gb_message_payload_len(resp.msg), 0, "Invalid response size");

	gb_message_dealloc(resp.msg);

	/* Handle receive credits request */
	resp = gb_transport_get_message();
	zassert_equal(resp.cport, 1, "Invalid cport");
	zassert_equal(gb_message_type(resp.msg), GB_UART_TYPE_RECEIVE_CREDITS,
		      "Invalid response type");
	zassert_equal(gb_message_payload_len(resp.msg), sizeof(*req_credits_data),
		      "Invalid response size");
	req_credits_data = (const struct gb_uart_receive_credits_request *)resp.msg->payload;
	zassert_equal(sys_le16_to_cpu(req_credits_data->count), TX_DATA_SIZE,
		      "Invalid receive credits request");

	gb_message_dealloc(resp.msg);

	ret = uart_emul_get_tx_data(dev, buf, ARRAY_SIZE(buf));
	zassert_equal(ret, TX_DATA_SIZE, "Invalid tx data");
	for (i = 0; i < TX_DATA_SIZE; i++) {
		zassert_equal(buf[i], i, "Invalid tx data");
	}
}

static void rx_data(uint8_t buf[], size_t len)
{
	struct gb_msg_with_cport req;
	const struct gb_uart_recv_data_request *req_data;
	size_t pos = 0;

	do {
		req = gb_transport_get_message();
		zassert_equal(req.cport, 1, "Invalid cport");
		zassert_equal(gb_message_type(req.msg), GB_UART_TYPE_RECEIVE_DATA,
			      "Invalid response type");
		zassert(gb_message_payload_len(req.msg) > sizeof(*req_data),
			"Invalid response size");

		req_data = (const struct gb_uart_recv_data_request *)req.msg->payload;

		memcpy(buf + pos, req_data->data, sys_le16_to_cpu(req_data->size));

		pos += sys_le16_to_cpu(req_data->size);

		gb_message_dealloc(req.msg);
	} while (pos < len);
}

ZTEST(greybus_uart_tests, test_receive_data)
{
	int i, ret;
	uint8_t buf[TX_DATA_SIZE];

	for (i = 0; i < TX_DATA_SIZE; i++) {
		buf[i] = i;
	}

	ret = uart_emul_put_rx_data(dev, buf, ARRAY_SIZE(buf));
	zassert_equal(ret, ARRAY_SIZE(buf), "Failed to write to uart");

	memset(buf, 0, sizeof(buf));
	rx_data(buf, ARRAY_SIZE(buf));

	for (i = 0; i < TX_DATA_SIZE; i++) {
		zassert_equal(buf[i], i, "Invalid tx data");
	}
}
