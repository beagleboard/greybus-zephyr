/*
 * Copyright (c) 2026 Ayush Singh, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "greybus/greybus_messages.h"
#include "greybus/greybus_protocols.h"
#include <zephyr/ztest.h>
#include <greybus/greybus.h>
#include <greybus-utils/manifest.h>
#include <greybus/greybus_log.h>
#include <greybus/service.h>
#include <greybus/apbridge.h>
#include <greybus/svc.h>

K_MSGQ_DEFINE(rx_msgq, sizeof(struct gb_msg_with_cport), 2, 1);

static int ap_send(struct gb_interface *intf, struct gb_message *msg, uint16_t cport)
{
	int ret;
	struct gb_message *resp;
	const struct gb_msg_with_cport msg_copy = {
		.cport = cport,
		.msg = gb_message_copy(msg),
	};

	if (cport == 0) {
		switch (gb_message_type(msg)) {
		case GB_SVC_TYPE_PROTOCOL_VERSION:
		case GB_SVC_TYPE_SVC_HELLO:
			resp = gb_message_response_alloc_from_req(NULL, 0, msg, GB_OP_SUCCESS);

			ret = gb_apbridge_send(AP_INF_ID, 0, resp);
			zassert_equal(ret, 0, "Failed to send response to SVC");

			gb_message_dealloc(msg);

			return 0;
		}
	}

	return k_msgq_put(&rx_msgq, &msg_copy, K_NO_WAIT);
}

static struct gb_interface intf = {
	.id = AP_INF_ID,
	.write = ap_send,
};

static void create_node_conn(void)
{
	int ret;
	struct gb_msg_with_cport msg;
	const struct gb_svc_conn_create_request req_data = {
		.intf1_id = AP_INF_ID,
		.cport1_id = 1,
		.intf2_id = INTF_START_ID,
		.cport2_id = 0,
	};
	struct gb_message *req = gb_message_request_alloc_with_payload(
		&req_data, sizeof(req_data), GB_SVC_TYPE_CONN_CREATE, false);

	ret = gb_apbridge_send(AP_INF_ID, 0, req);
	zassert_equal(ret, 0, "Failed to send request to node");

	ret = k_msgq_get(&rx_msgq, &msg, K_SECONDS(5));
	zassert_equal(ret, 0, "Expected get manifest response, got nothing");
	zassert(gb_message_is_response(msg.msg), "Expected get manifest response");
	zassert_equal(gb_message_type(msg.msg), GB_RESPONSE(GB_SVC_TYPE_CONN_CREATE),
		      "Expected get manifest response");
	zassert_equal(msg.cport, 0, "Expected response from SVC");
	zassert(gb_message_is_success(msg.msg), "get manifest request failed");
	zassert_equal(gb_message_payload_len(msg.msg), 0, "Unexpected payload length");

	gb_message_dealloc(msg.msg);
}

static void *greybus_standalone_tests_init(void)
{
	struct gb_message *resp;
	struct gb_msg_with_cport msg;
	const struct gb_svc_module_inserted_request *req_data;
	int ret;

	gb_interface_add(&intf);
	gb_apbridge_init();
	ret = gb_svc_init();
	zassert_equal(ret, 0, "Failed to initialize SVC");

	ret = greybus_service_init();
	zassert_equal(ret, 0, "Failed to register greybus service");

	ret = k_msgq_get(&rx_msgq, &msg, K_NO_WAIT);
	zassert_equal(ret, 0, "Expected module insterted message, got nothing");
	zassert_equal(msg.cport, 0, "Expected message from SVC");
	zassert_equal(gb_message_type(msg.msg), GB_SVC_TYPE_MODULE_INSERTED,
		      "Expected message from SVC");
	zassert_equal(gb_message_payload_len(msg.msg), sizeof(*req_data),
		      "Unexpected payload length");

	req_data = (const struct gb_svc_module_inserted_request *)msg.msg->payload;
	zassert_equal(req_data->primary_intf_id, INTF_START_ID, "Invalid interface ID");
	zassert_equal(req_data->intf_count, 1, "Invalid interface count");

	resp = gb_message_response_alloc_from_req(NULL, 0, msg.msg, GB_OP_SUCCESS);
	ret = gb_apbridge_send(AP_INF_ID, 0, resp);
	zassert_equal(ret, 0, "Failed to send response to SVC");

	gb_message_dealloc(msg.msg);

	create_node_conn();

	return NULL;
}

ZTEST_SUITE(greybus_standalone_tests, NULL, greybus_standalone_tests_init, NULL, NULL, NULL);

ZTEST(greybus_standalone_tests, test_cport_count)
{
	zassert_equal(GREYBUS_CPORT_COUNT, 1, "Invalid number of cports");
}

ZTEST(greybus_standalone_tests, test_get_manifest_size)
{
	int ret;
	struct gb_msg_with_cport msg;
	const struct gb_control_get_manifest_size_response *resp_data;
	struct gb_message *req =
		gb_message_request_alloc(0, GB_CONTROL_TYPE_GET_MANIFEST_SIZE, false);

	ret = gb_apbridge_send(AP_INF_ID, 1, req);
	zassert_equal(ret, 0, "Failed to send request to node");

	ret = k_msgq_get(&rx_msgq, &msg, K_SECONDS(5));
	zassert_equal(ret, 0, "Expected get manifest response, got nothing");
	zassert(gb_message_is_response(msg.msg), "Expected get manifest response");
	zassert_equal(msg.cport, 1, "Expected message from node");
	zassert(gb_message_is_success(msg.msg), "get manifest request failed");
	zassert_equal(gb_message_payload_len(msg.msg), sizeof(*resp_data),
		      "Unexpected payload length");

	resp_data = (const struct gb_control_get_manifest_size_response *)msg.msg->payload;
	zassert_equal(resp_data->size, manifest_size(), "Invalid manifest size");

	gb_message_dealloc(msg.msg);
}
