/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../greybus_transport.h"
#include <greybus/greybus.h>
#include <zephyr/kernel.h>
#include <greybus-utils/manifest.h>
#include "../greybus_internal.h"

K_MSGQ_DEFINE(rx_msgq, sizeof(struct gb_msg_with_cport), 2, 1);

static int init()
{
	return 0;
}

static int listen(uint16_t cport)
{
	return 0;
}

static int trans_send(uint16_t cport, const struct gb_message *msg)
{
	const struct gb_msg_with_cport msg_copy = {
		.cport = cport,
		.msg = gb_message_copy(msg),
	};

	return k_msgq_put(&rx_msgq, &msg_copy, K_NO_WAIT);
}

const struct gb_transport_backend gb_trans_backend = {
	.init = init,
	.listen = listen,
	.send = trans_send,
};

struct gb_msg_with_cport gb_transport_get_message(void)
{
	struct gb_msg_with_cport resp;

	k_msgq_get(&rx_msgq, &resp, K_FOREVER);

	return resp;
}
