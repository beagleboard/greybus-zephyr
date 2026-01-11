/*
 * Copyright (c) 2025 Ayush Singh, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "greybus/greybus_messages.h"
#include <zephyr/ztest.h>
#include <greybus/greybus.h>
#include <greybus-utils/manifest.h>

#define REQ_SIZE 256

struct gb_msg_with_cport gb_transport_get_message(void);

ZTEST_SUITE(greybus_loopback_tests, NULL, NULL, NULL, NULL, NULL);

ZTEST(greybus_loopback_tests, test_cport_count)
{
	zassert_equal(GREYBUS_CPORT_COUNT, 2, "Invalid number of cports");
}

ZTEST(greybus_loopback_tests, test_ping)
{
	struct gb_msg_with_cport resp;
	struct gb_message *req = gb_message_request_alloc(0, GB_LOOPBACK_TYPE_PING, false);

	greybus_rx_handler(1, req);
	resp = gb_transport_get_message();
	zassert_true(gb_message_is_success(resp.msg), "Greybus loopback ping failed");
	zassert_equal(gb_message_type(resp.msg), GB_RESPONSE(GB_LOOPBACK_TYPE_PING),
		      "Invalid request response");
	zassert_equal(gb_message_payload_len(resp.msg), 0,
		      "Greybus ping request should have empty response");

	gb_message_dealloc(resp.msg);
}

ZTEST(greybus_loopback_tests, test_sink)
{
	size_t i;
	struct gb_msg_with_cport resp;
	struct gb_message *req =
		gb_message_request_alloc(sizeof(struct gb_loopback_transfer_request) + REQ_SIZE,
					 GB_LOOPBACK_TYPE_SINK, false);
	struct gb_loopback_transfer_request *req_data =
		(struct gb_loopback_transfer_request *)req->payload;

	req_data->len = sys_cpu_to_le32(REQ_SIZE);
	for (i = 0; i < REQ_SIZE; i++) {
		req_data->data[i] = i;
	}

	greybus_rx_handler(1, req);
	resp = gb_transport_get_message();
	zassert_true(gb_message_is_success(resp.msg), "Greybus loopback sink failed");
	zassert_equal(gb_message_type(resp.msg), GB_RESPONSE(GB_LOOPBACK_TYPE_SINK),
		      "Invalid request response");
	zassert_equal(gb_message_payload_len(resp.msg), 0,
		      "Greybus sink request should have empty response");

	gb_message_dealloc(resp.msg);
}

ZTEST(greybus_loopback_tests, test_transfer)
{
	size_t i;
	struct gb_msg_with_cport resp;
	struct gb_message *req_copy;
	struct gb_message *req =
		gb_message_request_alloc(sizeof(struct gb_loopback_transfer_request) + REQ_SIZE,
					 GB_LOOPBACK_TYPE_TRANSFER, false);
	struct gb_loopback_transfer_request *req_data =
		(struct gb_loopback_transfer_request *)req->payload;

	req_data->len = sys_cpu_to_le32(REQ_SIZE);
	for (i = 0; i < REQ_SIZE; i++) {
		req_data->data[i] = i;
	}

	req_copy = gb_message_copy(req);
	greybus_rx_handler(1, req_copy);
	resp = gb_transport_get_message();
	zassert_true(gb_message_is_success(resp.msg), "Greybus loopback sink failed");
	zassert_equal(gb_message_type(resp.msg), GB_RESPONSE(GB_LOOPBACK_TYPE_TRANSFER),
		      "Invalid request response");
	zassert_equal(gb_message_payload_len(resp.msg), gb_message_payload_len(req),
		      "Greybus transfer request should have same size response");
	zassert_equal(memcmp(req->payload, resp.msg->payload, gb_message_payload_len(resp.msg)), 0,
		      "Response data should be same as request");

	gb_message_dealloc(req);
	gb_message_dealloc(resp.msg);
}

ZTEST(greybus_loopback_tests, test_zero_length_transfer)
{
	struct gb_msg_with_cport resp;
	struct gb_message *req = gb_message_request_alloc(
		sizeof(struct gb_loopback_transfer_request), GB_LOOPBACK_TYPE_TRANSFER, false);
	struct gb_loopback_transfer_request *req_data =
		(struct gb_loopback_transfer_request *)req->payload;

	req_data->len = sys_cpu_to_le32(0);
	// first we send an empty request
	greybus_rx_handler(1, req);
	resp = gb_transport_get_message(); // receiving
	zassert_true(gb_message_is_success(resp.msg), "Zero length transfer failed");
	zassert_equal(gb_message_type(resp.msg), GB_RESPONSE(GB_LOOPBACK_TYPE_TRANSFER),
		      "Invalid response type for zero-length transfer");
	zassert_equal(gb_message_payload_len(resp.msg), sizeof(struct gb_loopback_transfer_request),
		      "Response should contain header size");

	struct gb_loopback_transfer_request *resp_data =
		(struct gb_loopback_transfer_request *)resp.msg->payload;

	zassert_equal(sys_le32_to_cpu(resp_data->len), 0, "Response len field should be 0");
	// cleanup
	gb_message_dealloc(resp.msg);
	gb_message_dealloc(req);
}

ZTEST(greybus_loopback_tests, test_invalid_operation)
{
	struct gb_msg_with_cport resp;
	struct gb_message *req = gb_message_request_alloc(0, 0x7F, false);

	greybus_rx_handler(1, req);
	resp = gb_transport_get_message();

	// validation
	zassert_false(gb_message_is_success(resp.msg), "Should fail for invalid op");
	zassert_equal(gb_message_type(resp.msg), GB_RESPONSE(0x7F), "Type mismatch");
	gb_message_dealloc(resp.msg);
}

ZTEST(greybus_loopback_tests, test_rapid_pings)
{
	for (int i = 0; i < 20; i++) {
		struct gb_msg_with_cport resp;
		struct gb_message *req = gb_message_request_alloc(0, GB_LOOPBACK_TYPE_PING, false);
		greybus_rx_handler(1, req);
		resp = gb_transport_get_message();
		zassert_true(gb_message_is_success(resp.msg), "Ping %d failed in rapid fire", i);
		gb_message_dealloc(resp.msg);
	}
}
