/*
 * Copyright (c) 2026 Pavithra CP, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "greybus/greybus_messages.h"
#include "greybus/greybus_protocols.h"
#include <zephyr/ztest.h>
#include <greybus/greybus.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/byteorder.h>

// helper to retrieve messages from the transport layer
struct gb_msg_with_cport gb_transport_get_message(void);

static void *pwm_setup(void)
{
	struct gb_msg_with_cport resp;
	struct gb_control_version_request *ver_req;
	struct gb_message *msg;

	// Version handshake- initializes the Greybus Control Protocol
	msg = gb_message_request_alloc(sizeof(*ver_req), GB_CONTROL_TYPE_VERSION, false);
	ver_req = (struct gb_control_version_request *)msg->payload;
	ver_req->major = 0;
	ver_req->minor = 1;

	greybus_rx_handler(0, msg);
	resp = gb_transport_get_message();
	zassert_not_null(resp.msg, "No version response received");
	gb_message_dealloc(resp.msg);

	return NULL;
}

ZTEST_SUITE(greybus_pwm_tests, NULL, pwm_setup, NULL, NULL, NULL);

ZTEST(greybus_pwm_tests, test_pwm_count)
{
	struct gb_msg_with_cport resp;
	// CPort 1 is assigned to the first bundle (PWM) by the Greybus Manifest
	const int pwm_cport = 1;
	struct gb_message *req = gb_message_request_alloc(0, GB_PWM_TYPE_PWM_COUNT, false);

	greybus_rx_handler(pwm_cport, req);
	resp = gb_transport_get_message();
	if (resp.msg == NULL) {
		zassert_not_null(resp.msg, "Greybus stack did not respond on CPort %d", pwm_cport);
	}
	zassert_equal(resp.cport, pwm_cport, "Response received on wrong CPort");

	gb_message_dealloc(resp.msg);
}
