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
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "greybus_cport.h"
#include "greybus_transport.h"
#include <greybus-utils/manifest.h>
#include "greybus_internal.h"

LOG_MODULE_REGISTER(greybus, CONFIG_GREYBUS_LOG_LEVEL);

#define GB_PING_TYPE 0x00

/* 2 msg per cport seems to be a good number */
K_MSGQ_DEFINE(gb_rx_msgq, sizeof(struct gb_msg_with_cport), GREYBUS_CPORT_COUNT * 2, 1);

K_THREAD_STACK_DEFINE(gb_rx_thread_stack, 1280);
static struct k_thread gb_rx_thread;

uint8_t gb_errno_to_op_result(int err)
{
	switch (err) {
	case 0:
		return GB_OP_SUCCESS;

	case ENOMEM:
	case -ENOMEM:
		return GB_OP_NO_MEMORY;

	case EINTR:
	case -EINTR:
		return GB_OP_INTERRUPTED;

	case ETIMEDOUT:
	case -ETIMEDOUT:
		return GB_OP_TIMEOUT;

	case EPROTO:
	case -EPROTO:
	case ENOSYS:
	case -ENOSYS:
		return GB_OP_PROTOCOL_BAD;

	case EINVAL:
	case -EINVAL:
		return GB_OP_INVALID;

#ifndef EOVERFLOW
#define EOVERFLOW 75
#endif

	case EOVERFLOW:
	case -EOVERFLOW:
		return GB_OP_OVERFLOW;

	case ENODEV:
	case -ENODEV:
	case ENXIO:
	case -ENXIO:
		return GB_OP_NONEXISTENT;

	case EBUSY:
	case -EBUSY:
		return GB_OP_RETRY;

	default:
		return GB_OP_UNKNOWN_ERROR;
	}
}

static void gb_process_msg(struct gb_message *msg, uint16_t cport)
{
	const struct gb_cport *cport_ptr = gb_cport_get(cport);

	if (gb_message_type(msg) == GB_PING_TYPE) {
		return gb_transport_message_empty_response_send(msg, GB_OP_SUCCESS, cport);
	}

	cport_ptr->driver->op_handler(cport_ptr->priv, msg, cport);
}

static void gb_pending_message_worker(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	int ret;
	struct gb_msg_with_cport msg;

	while (1) {
		ret = k_msgq_get(&gb_rx_msgq, &msg, K_FOREVER);
		if (ret < 0) {
			continue;
		}

		LOG_DBG("CPort: %d, Type: %d, Result: %d, Id: %u", msg.cport,
			gb_message_type(msg.msg), msg.msg->header.result,
			msg.msg->header.operation_id);

		gb_process_msg(msg.msg, msg.cport);
	}
}

int greybus_rx_handler(uint16_t cport, struct gb_message *msg)
{
	const struct gb_driver *drv;
	const struct gb_msg_with_cport item = {
		.cport = cport,
		.msg = msg,
	};

	drv = gb_cport_get(cport)->driver;
	if (!drv || !drv->op_handler) {
		LOG_ERR("Cport %u does not have a valid driver registered", cport);
		gb_message_dealloc(msg);
		return 0;
	}
	// LOG_HEXDUMP_DBG(data, size, "RX: ");

	k_msgq_put(&gb_rx_msgq, &item, K_FOREVER);

	return 0;
}

int gb_listen(uint16_t cport)
{
	const struct gb_transport_backend *transport = gb_transport_get_backend();
	const struct gb_cport *cport_ptr = gb_cport_get(cport);

	if (!cport_ptr) {
		LOG_ERR("Invalid cport number %u", cport);
		return -EINVAL;
	}

	if (!cport_ptr->driver) {
		LOG_ERR("No driver registered! Can not connect CP%u.", cport);
		return -EINVAL;
	}

	return transport->listen(cport);
}

int gb_stop_listening(uint16_t cport)
{
	const struct gb_transport_backend *transport = gb_transport_get_backend();
	const struct gb_cport *cport_ptr = gb_cport_get(cport);

	if (!cport_ptr) {
		LOG_ERR("Invalid cport number %u", cport);
		return -EINVAL;
	}

	if (!cport_ptr->driver) {
		LOG_ERR("No driver registered! Can not disconnect CP%u.", cport);
		return -EINVAL;
	}

	return transport->stop_listening(cport);
}

int gb_init(const struct gb_transport_backend *transport)
{
	int ret;

	if (!transport) {
		return -EINVAL;
	}

	ret = gb_cports_init();
	if (ret < 0) {
		return ret;
	}

	k_thread_create(&gb_rx_thread, gb_rx_thread_stack,
			K_THREAD_STACK_SIZEOF(gb_rx_thread_stack), gb_pending_message_worker, NULL,
			NULL, NULL, 5, 0, K_NO_WAIT);

	transport->init();

	return 0;
}

void gb_deinit(void)
{
	const struct gb_transport_backend *transport = gb_transport_get_backend();

	if (!transport) {
		return; /* gb not initialized */
	}

	k_thread_abort(&gb_rx_thread);

	gb_cports_deinit();

	if (transport->exit) {
		transport->exit();
	}
}

int gb_notify(uint16_t cport, enum gb_event event)
{
	const struct gb_cport *cport_ptr = gb_cport_get(cport);

	if (!cport_ptr) {
		return -EINVAL;
	}

	if (!cport_ptr->driver) {
		return -ENOTCONN;
	}

	switch (event) {
	case GB_EVT_CONNECTED:
		if (cport_ptr->driver->connected) {
			cport_ptr->driver->connected(cport_ptr->priv, cport);
		}
		break;

	case GB_EVT_DISCONNECTED:
		if (cport_ptr->driver->disconnected) {
			cport_ptr->driver->disconnected(cport_ptr->priv);
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}
