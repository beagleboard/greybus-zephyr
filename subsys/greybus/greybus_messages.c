// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2023-25 Ayush Singh <ayushdevel1325@gmail.com>
 */

#include <greybus/greybus_messages.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "greybus_heap.h"

#define OPERATION_ID_START 1

LOG_MODULE_REGISTER(greybus_messages, CONFIG_GREYBUS_LOG_LEVEL);

static atomic_t operation_id_counter = ATOMIC_INIT(OPERATION_ID_START);

uint16_t new_operation_id(void)
{
	atomic_val_t temp = atomic_inc(&operation_id_counter);

	if (temp == UINT16_MAX) {
		atomic_set(&operation_id_counter, OPERATION_ID_START);
	}
	return temp;
}

struct gb_message *gb_message_alloc(size_t payload_len, uint8_t message_type, uint16_t operation_id,
				    uint8_t status)
{
	struct gb_message *msg;

	msg = gb_alloc(sizeof(struct gb_message) + payload_len);
	if (msg == NULL) {
		LOG_WRN("Failed to allocate Greybus request message");
		return NULL;
	}

	msg->header.size = sys_cpu_to_le16(sizeof(struct gb_operation_msg_hdr) + payload_len);
	msg->header.operation_id = sys_cpu_to_le16(operation_id);
	msg->header.type = message_type;
	msg->header.result = status;

	return msg;
}

void gb_message_dealloc(struct gb_message *msg)
{
	gb_free(msg);
}

struct gb_message *gb_message_request_alloc(size_t payload_len, uint8_t request_type,
					    bool is_oneshot)
{
	uint16_t operation_id = is_oneshot ? 0 : new_operation_id();

	struct gb_message *msg = gb_message_alloc(payload_len, request_type, operation_id, 0);

	return msg;
}
