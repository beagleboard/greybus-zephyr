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
 */

#include "greybus_transport.h"
#include <zephyr/logging/log.h>
#include <greybus/greybus_protocols.h>
#include "greybus_internal.h"

LOG_MODULE_REGISTER(greybus_loopback, CONFIG_GREYBUS_LOG_LEVEL);

static void gb_loopback_transfer_req_cb(struct gb_message *req, uint16_t cport)
{
	req->header.type = GB_RESPONSE(GB_LOOPBACK_TYPE_TRANSFER);
	req->header.result = GB_OP_SUCCESS;

	gb_transport_message_send(req, cport);

	gb_message_dealloc(req);
}

static void gb_loopback_handler(const void *priv, struct gb_message *msg, uint16_t cport)
{
	ARG_UNUSED(priv);

	switch (gb_message_type(msg)) {
	case GB_LOOPBACK_TYPE_PING:
		return gb_transport_message_empty_response_send(msg, GB_OP_SUCCESS, cport);
	case GB_LOOPBACK_TYPE_TRANSFER:
		return gb_loopback_transfer_req_cb(msg, cport);
	case GB_LOOPBACK_TYPE_SINK:
		return gb_transport_message_empty_response_send(msg, GB_OP_SUCCESS, cport);
	default:
		LOG_ERR("Invalid type");
		gb_transport_message_empty_response_send(msg, GB_OP_INVALID, cport);
	}
}

const struct gb_driver gb_loopback_driver = {
	.op_handler = gb_loopback_handler,
};
