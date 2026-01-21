/*
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <zephyr/init.h>
#include <greybus/greybus.h>
#include <greybus-utils/manifest.h>
#include "../greybus_transport.h"
#include <zephyr/logging/log.h>
#include "../greybus_internal.h"

LOG_MODULE_REGISTER(greybus_service, CONFIG_GREYBUS_LOG_LEVEL);

#include "certificate.h"

int greybus_service_init(void)
{
	int r;
	const struct gb_transport_backend *xport = gb_transport_get_backend();

	r = greybus_tls_init();
	if (r < 0) {
		LOG_ERR("gb_tls_init() failed: %d", r);
		return r;
	}
	LOG_DBG("Greybus initializing..");

	if (GREYBUS_CPORT_COUNT == 0) {
		LOG_ERR("no cports are defined");
		return -EINVAL;
	}

	r = gb_init(xport);
	if (r < 0) {
		LOG_ERR("gb_init() failed: %d", r);
		return r;
	}

	LOG_INF("Greybus is active");
	return 0;
}

#ifdef CONFIG_GREYBUS_SERVICE
SYS_INIT(greybus_service_init, APPLICATION, CONFIG_GREYBUS_SERVICE_INIT_PRIORITY);
#endif // CONFIG_GREYBUS_SERVICE
