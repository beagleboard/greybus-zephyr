/*
 * Copyright (c) 2025 Ayush Singh, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GREYBUS_INTERNAL_H_
#define _GREYBUS_INTERNAL_H_

#include <greybus/greybus.h>

typedef void (*gb_operation_handler_t)(const void *priv, struct gb_message *msg, uint16_t cport);

struct gb_driver {
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

/**
 * Initialize greybus.
 *
 * @param transport: greybus transport backend pointer.
 *
 * @return 0 in case of success.
 * @return < 0 in case of error.
 */
int gb_init(const struct gb_transport_backend *transport);

/**
 * De-initialize greybus.
 */
void gb_deinit(void);

#endif // _GREYBUS_INTERNAL_H_
