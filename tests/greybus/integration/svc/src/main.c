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

// struct gb_msg_with_cport gb_transport_get_message(void);

ZTEST_SUITE(greybus_svc_tests, NULL, NULL, NULL, NULL, NULL);
