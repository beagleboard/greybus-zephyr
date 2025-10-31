/*
 * Copyright (c) 2025 Ayush Singh, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <greybus/greybus_protocols.h>
#include "greybus_transport.h"
#include "greybus_internal.h"
#include "greybus_raw_internal.h"
#include <zephyr/kernel.h>
#include <greybus-utils/manifest.h>
#include "greybus_cport.h"
#include <greybus/greybus.h>
#include "greybus-manifest.h"

static void gb_raw_send_handler(uint16_t cport, struct gb_message *req,
				const struct gb_raw_driver_data *data)
{
	int ret;
	const struct gb_raw_send_request *req_data =
		(const struct gb_raw_send_request *)req->payload;

	ret = data->cb(sys_le32_to_cpu(req_data->len), req_data->data, data->cb_priv);

	gb_transport_message_empty_response_send(req, ret, cport);
}

static void op_handler(const void *priv, struct gb_message *msg, uint16_t cport)
{
	const struct gb_raw_driver_data *data = priv;

	if (!data->cb) {
		return gb_transport_message_empty_response_send(msg, GB_OP_INTERNAL, cport);
	}

	switch (gb_message_type(msg)) {
	case GB_RAW_TYPE_SEND:
		return gb_raw_send_handler(cport, msg, data);
	case GB_RESPONSE(GB_RAW_TYPE_SEND):
		return gb_message_dealloc(msg);
	default:
		return gb_transport_message_empty_response_send(msg, GB_OP_PROTOCOL_BAD, cport);
	}
}

const struct gb_driver gb_raw_driver = {
	.op_handler = op_handler,
};

int greybus_raw_register(greybus_raw_cb_t cb, void *priv)
{
	int i;
	const struct gb_cport *cport;
	struct gb_raw_driver_data *data;

	for (i = 0; i < GREYBUS_RAW_CPORT_COUNT; ++i) {
		cport = gb_cport_get(GREYBUS_RAW_CPORT_START + i);

		__ASSERT(cport->protocol == GREYBUS_PROTOCOL_RAW,
			 "Cport does not support raw protocol");

		data = (struct gb_raw_driver_data *)cport->priv;
		if (!data->cb) {
			data->cb = cb;
			data->cb_priv = priv;

			return i;
		}
	}

	return -ENODEV;
}

int greybus_raw_send_data(uint16_t id, uint32_t len, const uint8_t *data)
{
	int ret;
	uint16_t cport_id = GREYBUS_RAW_CPORT_START + id;
	struct gb_raw_send_request *req_data;
	struct gb_message *msg =
		gb_message_request_alloc(sizeof(*req_data) + len, GB_RAW_TYPE_SEND, false);

	req_data = (struct gb_raw_send_request *)msg->payload;

	req_data->len = sys_cpu_to_le32(len);
	memcpy(req_data->data, data, len);

	ret = gb_transport_message_send(msg, cport_id);
	gb_message_dealloc(msg);

	return ret;
}
