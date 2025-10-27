/*
 * Copyright (c) 2025 Ayush Singh, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "greybus/greybus_messages.h"
#include <zephyr/ztest.h>
#include <greybus/greybus.h>
#include <greybus-utils/manifest.h>
#include <greybus/greybus_log.h>

struct gb_msg_with_cport gb_transport_get_message(void);

ZTEST_SUITE(greybus_log_tests, NULL, NULL, NULL, NULL, NULL);

ZTEST(greybus_log_tests, test_cport_count)
{
	zassert_equal(GREYBUS_CPORT_COUNT, 2, "Invalid number of cports");
}

ZTEST(greybus_log_tests, test_send_log)
{
	int ret;
	struct gb_msg_with_cport req;
	struct gb_message *resp;
	const char msg[] = "TEST MESSAGE";
	const struct gb_log_send_log_request *req_data;

	gb_log_send_log(strlen(msg), msg);

	req = gb_transport_get_message();
	zassert_equal(req.cport, 1, "Incorrect cport");
	zassert_true(gb_message_is_success(req.msg), "Greybus loopback ping failed");
	zassert_equal(gb_message_type(req.msg), GB_LOG_TYPE_SEND_LOG, "Invalid request response");
	zassert_equal(gb_message_payload_len(req.msg),
		      sizeof(struct gb_log_send_log_request) + sizeof(msg),
		      "Greybus ping request should have empty response");

	req_data = (const struct gb_log_send_log_request *)req.msg->payload;

	zassert_equal(sys_le16_to_cpu(req_data->len), sizeof(msg), "Incorrect msg length");
	zassert_equal(memcmp(req_data->msg, msg, sizeof(msg)), 0, "Incorrect msg string");

	resp = gb_message_response_alloc_from_req(NULL, 0, req.msg, GB_OP_SUCCESS);
	zassert_not_null(resp, "Failed to allocate response");

	ret = greybus_rx_handler(1, resp);
	zassert_equal(ret, 0, "Sending response failed");

	gb_message_dealloc(req.msg);
}
