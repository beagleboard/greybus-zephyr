/*
 * Copyright (c) 2025 Ayush Singh, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <greybus/greybus_protocols.h>
#include "greybus_transport.h"
#include <greybus-utils/manifest.h>
#include "greybus_internal.h"

static void op_handler(const void *priv, struct gb_message *msg, uint16_t cport)
{
	ARG_UNUSED(priv);

	switch (gb_message_type(msg)) {
	case GB_RESPONSE(GB_LOG_TYPE_SEND_LOG):
		return gb_message_dealloc(msg);
	default:
		return gb_transport_message_empty_response_send(msg, GB_OP_PROTOCOL_BAD, cport);
	}
}

void gb_log_send_log(uint16_t len, const char *log)
{
	struct gb_log_send_log_request *req_data;
	struct gb_message *msg =
		gb_message_request_alloc(sizeof(*req_data) + len + 1, GB_LOG_TYPE_SEND_LOG, false);

	if (!msg) {
		return;
	}

	req_data = (struct gb_log_send_log_request *)msg->payload;

	/* Include NULL terminator */
	req_data->len = sys_cpu_to_le16(len + 1);
	memcpy(req_data->msg, log, len);
	req_data->msg[len] = '\0';

	gb_transport_message_send(msg, GREYBUS_LOG_CPORT);
	gb_message_dealloc(msg);
}

const struct gb_driver gb_log_driver = {
	.op_handler = op_handler,
};
