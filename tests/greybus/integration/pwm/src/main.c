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
#include <zephyr/logging/log.h>
#include <zephyr/fff.h>

DEFINE_FFF_GLOBALS;
LOG_MODULE_REGISTER(greybus_pwm_test, CONFIG_GREYBUS_LOG_LEVEL);

struct gb_msg_with_cport gb_transport_get_message(void);

static void *pwm_setup(void)
{
	struct gb_msg_with_cport resp;
	struct gb_control_version_request *ver_req;
	struct gb_message *msg;

	/*
	 * Greybus Control Protocol Handshake.
	 * We (the Test/Host) must send a Version Request to transition the
	 * Greybus service from 'Initialized' to 'Active'.
	 * Without this, the stack may reject requests on other CPorts.
	 */
	msg = gb_message_request_alloc(sizeof(*ver_req), GB_CONTROL_TYPE_VERSION, false);
	ver_req = (struct gb_control_version_request *)msg->payload;
	ver_req->major = 0;
	ver_req->minor = 1;

	greybus_rx_handler(0, msg);
	resp = gb_transport_get_message();
	zassert_not_null(resp.msg, "No version response received");

	zassert_true(gb_message_is_success(resp.msg), "Version Handshake failed");

	gb_message_dealloc(resp.msg);
	return NULL;
}

ZTEST_SUITE(greybus_pwm_tests, NULL, pwm_setup, NULL, NULL, NULL);

/*CPort 1 is assigned to the first bundle (PWM) by the Greybus Manifest */
static const int pwm_cport = 1;

ZTEST(greybus_pwm_tests, test_pwm_count)
{
	struct gb_msg_with_cport resp;
	struct gb_message *req;
	const struct gb_pwm_count_response *pwm_resp;

	req = gb_message_request_alloc(0, GB_PWM_TYPE_PWM_COUNT, false);

	greybus_rx_handler(pwm_cport, req);
	resp = gb_transport_get_message();

	/*transport check */
	if (resp.msg == NULL) {
		zassert_not_null(resp.msg, "Greybus stack did not respond on CPort %d", pwm_cport);
	}
	zassert_equal(resp.cport, pwm_cport, "Response received on wrong CPort");

	zassert_equal(resp.msg->header.type, GB_RESPONSE(GB_PWM_TYPE_PWM_COUNT),
		      "Wrong response type");
	zassert_true(gb_message_is_success(resp.msg), "Operation failed with error: %d",
		     resp.msg->header.result);

	pwm_resp = (struct gb_pwm_count_response *)resp.msg->payload;

	/* The vnd,pwm driver on native_sim might report 0 channels depending on the overlay
	 * configuration. We verify that the value is readable (greater than 0).*/
	LOG_INF("PWM Driver reported %d channels", pwm_resp->count);

	/* The current driver on native_sim has 1 channel hardcoded.
	 * Greybus spec says count = num_channels-1
	 * So we expect count==0.
	 * This will fail with multichannel support
	 */
	zassert_equal(pwm_resp->count, 0, "Expected 1 channel (count=0), got %d", pwm_resp->count);
	gb_message_dealloc(resp.msg);
}

ZTEST(greybus_pwm_tests, test_pwm_config)
{
	struct gb_message *req;
	struct gb_msg_with_cport resp;
	struct gb_pwm_config_request *cfg_req;

	req = gb_message_request_alloc(sizeof(*cfg_req), GB_PWM_TYPE_CONFIG, false);
	cfg_req = (struct gb_pwm_config_request *)req->payload;

	/* Standard Test Config: 1 kHz Frequency, 50% Duty Cycle
	 * Standard for verify basic PWM operation.
	 */
	cfg_req->which = 0;
	cfg_req->duty = sys_cpu_to_le32(500000);
	cfg_req->period = sys_cpu_to_le32(1000000);

	greybus_rx_handler(pwm_cport, req);
	resp = gb_transport_get_message();

	zassert_not_null(resp.msg, "No config response received");
	zassert_equal(resp.msg->header.type, GB_RESPONSE(GB_PWM_TYPE_CONFIG), "Wrong Type");

	zassert_true(gb_message_is_success(resp.msg), "Config failed: %d", resp.msg->header.result);
	gb_message_dealloc(resp.msg);
}

ZTEST(greybus_pwm_tests, test_pwm_enable)
{
	struct gb_message *req;
	struct gb_msg_with_cport resp;
	struct gb_pwm_enable_request *en_req;

	req = gb_message_request_alloc(sizeof(*en_req), GB_PWM_TYPE_ENABLE, false);
	en_req = (struct gb_pwm_enable_request *)req->payload;
	en_req->which = 0; /*enabling channel 0*/

	greybus_rx_handler(pwm_cport, req);
	resp = gb_transport_get_message();

	zassert_not_null(resp.msg, "No enable response received");
	zassert_equal(resp.msg->header.type, GB_RESPONSE(GB_PWM_TYPE_ENABLE), "Wrong Type");

	zassert_true(gb_message_is_success(resp.msg), "Enable failed: %d", resp.msg->header.result);

	gb_message_dealloc(resp.msg);
}

ZTEST(greybus_pwm_tests, test_pwm_disable)
{
	struct gb_message *req;
	struct gb_msg_with_cport resp;
	struct gb_pwm_disable_request *dis_req;

	req = gb_message_request_alloc(sizeof(*dis_req), GB_PWM_TYPE_DISABLE, false);
	dis_req = (struct gb_pwm_disable_request *)req->payload;
	dis_req->which = 0;

	greybus_rx_handler(pwm_cport, req);
	resp = gb_transport_get_message();

	zassert_not_null(resp.msg, "No disable response received");
	zassert_equal(resp.msg->header.type, GB_RESPONSE(GB_PWM_TYPE_DISABLE), "Wrong Type");

	zassert_true(gb_message_is_success(resp.msg), "Disable failed: %d",
		     resp.msg->header.result);

	gb_message_dealloc(resp.msg);
}

ZTEST(greybus_pwm_tests, test_pwm_polarity)
{
	struct gb_message *req;
	struct gb_msg_with_cport resp;
	struct gb_pwm_polarity_request *pol_req;

	req = gb_message_request_alloc(sizeof(*pol_req), GB_PWM_TYPE_POLARITY, false);
	pol_req = (struct gb_pwm_polarity_request *)req->payload;
	pol_req->which = 0;
	pol_req->polarity = 1; /*Inversed*/

	greybus_rx_handler(pwm_cport, req);
	resp = gb_transport_get_message();

	zassert_not_null(resp.msg, "No polarity response received");
	zassert_equal(resp.msg->header.type, GB_RESPONSE(GB_PWM_TYPE_POLARITY), "Wrong Type");

	zassert_true(gb_message_is_success(resp.msg), "Polarity failed: %d",
		     resp.msg->header.result);

	gb_message_dealloc(resp.msg);
}
