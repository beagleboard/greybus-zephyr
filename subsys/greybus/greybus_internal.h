/*
 * Copyright (c) 2025 Ayush Singh, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GREYBUS_INTERNAL_H_
#define _GREYBUS_INTERNAL_H_

#include <greybus/greybus_messages.h>

typedef void (*gb_operation_handler_t)(const void *priv, struct gb_message *msg, uint16_t cport);

struct gb_driver {
	/*
	 * This is the callback in which all the initialization of driver-specific
	 * data should be done. The driver should create struct device and assign
	 * it to bundle->dev. The driver should store any private data it may need
	 * in bundle->priv. The same bundle object will be passed to the driver in
	 * all subsequent greybus handler callbacks calls.
	 */
	int (*init)(const void *priv, uint16_t cport);
	/*
	 * This function is called upon driver deregistration. All private data
	 * stored in bundle should be freed and struct device should be closed.
	 */
	void (*exit)(const void *priv);
	void (*connected)(const void *priv, uint16_t cport);
	void (*disconnected)(const void *priv);

	gb_operation_handler_t op_handler;
};

enum gb_event {
	GB_EVT_CONNECTED,
	GB_EVT_DISCONNECTED,
};

int gb_listen(uint16_t cport);
int gb_stop_listening(uint16_t cport);
int gb_notify(uint16_t cport, enum gb_event event);

uint8_t gb_errno_to_op_result(int err);

#endif // _GREYBUS_INTERNAL_H_
