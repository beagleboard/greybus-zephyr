/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../greybus_transport.h"
#include <greybus/greybus.h>
#include <zephyr/kernel.h>
#include <greybus-utils/manifest.h>
#include <greybus/svc.h>
#include <greybus/apbridge.h>

static int gb_intf_send(struct gb_interface *intf, struct gb_message *msg, uint16_t cport)
{
	return greybus_rx_handler(cport, msg);
}

static struct gb_interface intf = {
	.id = INTF_START_ID,
	.write = gb_intf_send,
};

static int gb_trans_init()
{
	gb_interface_add(&intf);
	return gb_svc_send_module_inserted(INTF_START_ID, 1, 0);
}

static int gb_trans_listen(uint16_t cport)
{
	ARG_UNUSED(cport);

	return 0;
}

static int gb_trans_send(uint16_t cport, const struct gb_message *msg)
{
	struct gb_message *msg_copy = gb_message_copy(msg);

	return gb_apbridge_send(INTF_START_ID, cport, msg_copy);
}

const struct gb_transport_backend gb_trans_backend = {
	.init = gb_trans_init,
	.listen = gb_trans_listen,
	.send = gb_trans_send,
};
