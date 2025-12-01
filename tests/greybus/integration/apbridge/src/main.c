/*
 * Copyright (c) 2025 Ayush Singh, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <greybus/apbridge.h>
#include <zephyr/ztest.h>

ZTEST_SUITE(greybus_apbridge_tests, NULL, NULL, NULL, NULL, NULL);

ZTEST(greybus_apbridge_tests, test_intf_free)
{
	struct gb_interface *intf1, *intf2;

	intf1 = gb_interface_alloc(NULL, NULL, NULL, NULL);

	zassert_not_null(intf1, "Failed to allocate greybus interface");
	zassert_equal(intf1->id, 2, "0 and 1 should be reserved for AP and SVC");

	intf2 = gb_interface_alloc(NULL, NULL, NULL, NULL);
	zassert_not_null(intf2, "Failed to allocate greybus interface");
	zassert_equal(intf2->id, 3, "3 should be the next free interface id");

	gb_interface_dealloc(intf1);

	intf1 = gb_interface_alloc(NULL, NULL, NULL, NULL);

	zassert_not_null(intf1, "Failed to allocate greybus interface");
	zassert_equal(intf1->id, 2, "2 Should be free again");

	gb_interface_dealloc(intf1);
	gb_interface_dealloc(intf2);
}

ZTEST(greybus_apbridge_tests, test_intf_overflow)
{
	int i;
	struct gb_interface *intfs[CONFIG_GREYBUS_APBRIDGE_CPORTS];
	struct gb_interface *intf;

	for (i = 2; i < CONFIG_GREYBUS_APBRIDGE_CPORTS; i++) {
		intfs[i] = gb_interface_alloc(NULL, NULL, NULL, NULL);

		zassert_not_null(intfs[i], "Failed to allocate greybus interface");
		zassert_equal(intfs[i]->id, i, "Invalid ID");
	}

	intf = gb_interface_alloc(NULL, NULL, NULL, NULL);
	zassert_is_null(intf, "Should overflow");

	for (i = 2; i < CONFIG_GREYBUS_APBRIDGE_CPORTS; i++) {
		gb_interface_dealloc(intfs[i]);
	}
}

ZTEST(greybus_apbridge_tests, test_multi_add)
{
	int ret;
	struct gb_interface intf = {
		.id = AP_INF_ID,
	};

	ret = gb_interface_add(&intf);
	zassert_equal(ret, 0, "Failed to add AP");

	ret = gb_interface_add(&intf);
	zassert_equal(ret, -EALREADY, "Failed to add AP");

	gb_interface_remove(AP_INF_ID);
}

static int write_cb(struct gb_interface *intf, struct gb_message *msg, uint16_t cport)
{
	intf->ctrl_data = msg;

	return 0;
}

ZTEST(greybus_apbridge_tests, test_message_send)
{
	int ret;
	struct gb_message msg;
	struct gb_interface ap_intf = {
		.id = AP_INF_ID,
		.write = write_cb,
		.create_connection = NULL,
		.destroy_connection = NULL,
		.ctrl_data = NULL,
	};
	struct gb_interface svc_intf = {
		.id = 0,
		.write = write_cb,
		.create_connection = NULL,
		.destroy_connection = NULL,
		.ctrl_data = NULL,
	};

	ret = gb_interface_add(&ap_intf);
	zassert_equal(ret, 0, "Failed to add AP");

	ret = gb_interface_add(&svc_intf);
	zassert_equal(ret, 0, "Failed to add SVC");

	ret = gb_apbridge_connection_create(AP_INF_ID, 0, 0, 0);
	zassert_equal(ret, 0, "Failed to create connection");

	ret = gb_apbridge_send(AP_INF_ID, 0, &msg);
	zassert_equal(ret, 0, "Failed to send message");
	zassert_equal_ptr(&msg, svc_intf.ctrl_data, "Should point to the same message");

	ret = gb_apbridge_send(0, 0, &msg);
	zassert_equal(ret, 0, "Failed to send message");
	zassert_equal_ptr(&msg, ap_intf.ctrl_data, "Should point to the same message");

	ret = gb_apbridge_connection_destroy(AP_INF_ID, 0, 0, 0);
	zassert_equal(ret, 0, "Failed to create connection");

	gb_interface_remove(AP_INF_ID);
	gb_interface_remove(0);
}
