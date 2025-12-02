/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 */

#ifndef _GREYBUS_MESSAGES_H_
#define _GREYBUS_MESSAGES_H_

#include <zephyr/types.h>
#include <zephyr/sys/byteorder.h>
#include <greybus/greybus_protocols.h>

enum gb_operation_type {
	GB_TYPE_RESPONSE_FLAG = 0x80,
};

#define GB_RESPONSE(req) (req | GB_TYPE_RESPONSE_FLAG)

enum gb_operation_result {
	GB_OP_SUCCESS = 0x00,
	GB_OP_INTERRUPTED = 0x01,
	GB_OP_TIMEOUT = 0x02,
	GB_OP_NO_MEMORY = 0x03,
	GB_OP_PROTOCOL_BAD = 0x04,
	GB_OP_OVERFLOW = 0x05,
	GB_OP_INVALID = 0x06,
	GB_OP_RETRY = 0x07,
	GB_OP_NONEXISTENT = 0x08,
	GB_OP_UNKNOWN_ERROR = 0xfe,
	GB_OP_INTERNAL = 0xff,
};

/*
 * Struct to represent greybus message. This is a variable sized type.
 *
 * @param header: greybus msg header.
 * @param payload_size: size of payload in bytes
 * @param payload: heap allocated payload.
 */
struct gb_message {
	struct gb_operation_msg_hdr header;
	uint8_t payload[];
};

/*
 * Return the paylaod length of a greybus message from message header.
 *
 * @param hdr: greybus header
 *
 * @return payload length
 */
static inline size_t gb_hdr_payload_len(const struct gb_operation_msg_hdr *hdr)
{
	return sys_le16_to_cpu(hdr->size) - sizeof(struct gb_operation_msg_hdr);
}

/*
 * Return the paylaod length of a greybus message.
 *
 * @param msg: Greybus message
 *
 * @return payload length
 */
static inline size_t gb_message_payload_len(const struct gb_message *msg)
{
	return gb_hdr_payload_len(&msg->header);
}

/*
 * Check if the greybus message header is a response.
 *
 * @param hdr: greybus header
 *
 * @return true if message is response, else false.
 */
static inline bool gb_hdr_is_response(const struct gb_operation_msg_hdr *hdr)
{
	return hdr->type & GB_TYPE_RESPONSE_FLAG;
}

/*
 * Check if the greybus message header is a successful.
 *
 * @param hdr: greybus header
 *
 * @return true if message is successful, else false.
 */
static inline bool gb_hdr_is_success(const struct gb_operation_msg_hdr *hdr)
{
	return hdr->result == GB_OP_SUCCESS;
}

/*
 * Check if the greybus message is a response.
 *
 * @param msg: greybus message
 *
 * @return true if message is response, else false.
 */
static inline bool gb_message_is_response(const struct gb_message *msg)
{
	return gb_hdr_is_response(&msg->header);
}

/*
 * Check if the greybus message is a successful.
 *
 * @param msg: greybus message
 *
 * @return true if message is successful, else false.
 */
static inline bool gb_message_is_success(const struct gb_message *msg)
{
	return gb_hdr_is_success(&msg->header);
}

/*
 * Allocate Greybus message
 *
 * @param Payload len
 * @param Response Type
 * @param Operation ID of Request
 * @param Status
 *
 * @return greybus message allocated on heap. Null in case of error
 */
struct gb_message *gb_message_alloc(size_t payload_len, uint8_t message_type, uint16_t operation_id,
				    uint8_t status);

/*
 * Deallocate a greybus message.
 *
 * @param pointer to the message to deallcate
 */
void gb_message_dealloc(struct gb_message *msg);

/*
 * Allocate a greybus request message
 *
 * @param Payload len
 * @param Request Type
 * @param Is one shot
 *
 * @return greybus message allocated on heap. Null in case of error
 */
struct gb_message *gb_message_request_alloc(size_t payload_len, uint8_t request_type,
					    bool is_oneshot);

/*
 * Allocate a greybus request message with payload
 *
 * @param payload
 * @param Payload len
 * @param Request Type
 * @param Is one shot
 *
 * @return greybus message allocated on heap. Null in case of error
 */
static inline struct gb_message *gb_message_request_alloc_with_payload(const void *payload,
								       size_t payload_len,
								       uint8_t request_type,
								       bool is_oneshot)
{
	struct gb_message *req = gb_message_request_alloc(payload_len, request_type, is_oneshot);

	if (req) {
		memcpy(req->payload, payload, payload_len);
	}

	return req;
}

/*
 * Allocate a greybus response message
 *
 * @param Payload
 * @param Payload len
 * @param Request Type
 * @param Operation ID of Request
 * @param Status
 *
 * @return greybus message allocated on heap. Null in case of error
 */
static inline struct gb_message *gb_message_response_alloc(const void *payload, size_t payload_len,
							   uint8_t request_type,
							   uint16_t operation_id, uint8_t status)
{
	struct gb_message *msg =
		gb_message_alloc(payload_len, GB_RESPONSE(request_type), operation_id, status);
	memcpy(msg->payload, payload, payload_len);
	return msg;
}

static inline uint8_t gb_message_type(const struct gb_message *msg)
{
	return msg->header.type;
}

/**
 * Helper to allocate response for a given message
 *
 * @param payload
 * @param payload_len
 * @param req Associated grebus request message
 * @param status
 */
static inline struct gb_message *gb_message_response_alloc_from_req(const void *payload,
								    size_t payload_len,
								    struct gb_message *req,
								    uint8_t status)
{
	return gb_message_response_alloc(payload, payload_len, gb_message_type(req),
					 req->header.operation_id, status);
}

/**
 * Helper to create copy of greybus message.
 *
 * @parm msg
 */
static inline struct gb_message *gb_message_copy(const struct gb_message *msg)
{
	size_t payload_len = gb_message_payload_len(msg);
	struct gb_message *resp = gb_message_alloc(payload_len, gb_message_type(msg),
						   msg->header.operation_id, msg->header.result);

	memcpy(resp->payload, msg->payload, payload_len);

	return resp;
}

/**
 * Helper to get a new unique operation id.
 */
uint16_t new_operation_id(void);

#endif
