/*
 * Copyright (c) 2015 Google Inc.
 * Copyright (c) 2025 Ayush Singh, BeagleBoard.org
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Viresh Kumar <viresh.kumar@linaro.org>
 */

#include <greybus-utils/manifest.h>
#include "greybus_transport.h"
#include <zephyr/logging/log.h>
#include <greybus/greybus_protocols.h>
#include "greybus_internal.h"

LOG_MODULE_REGISTER(greybus_control, CONFIG_GREYBUS_LOG_LEVEL);

#define GB_CONTROL_VERSION_MAJOR 0
#define GB_CONTROL_VERSION_MINOR 1

static void gb_control_protocol_version(uint16_t cport, struct gb_message *req)
{
	const struct gb_control_version_request resp_data = {
		.major = GB_CONTROL_VERSION_MAJOR,
		.minor = GB_CONTROL_VERSION_MINOR,
	};

	gb_transport_message_response_success_send(req, &resp_data, sizeof(resp_data), cport);
}

static void gb_control_get_manifest_size(uint16_t cport, struct gb_message *req)
{
	const struct gb_control_get_manifest_size_response resp_data = {
		.size = sys_cpu_to_le16(manifest_size()),
	};

	gb_transport_message_response_success_send(req, &resp_data, sizeof(resp_data), cport);
}

static void gb_control_get_manifest(uint16_t cport, struct gb_message *req)
{
	struct gb_message *msg = gb_message_alloc(manifest_size(), GB_RESPONSE(req->header.type),
						  req->header.operation_id, GB_OP_SUCCESS);

	manifest_create(msg->payload, manifest_size());

	gb_transport_message_send(msg, cport);

	gb_message_dealloc(msg);
}

static void gb_control_connected(uint16_t cport, struct gb_message *req)
{
	int retval;
	const struct gb_control_connected_request *req_data =
		(const struct gb_control_connected_request *)req->payload;
	uint16_t target_cport = sys_le16_to_cpu(req_data->cport_id);

	if (gb_message_payload_len(req) < sizeof(*req_data)) {
		LOG_ERR("dropping short message");
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}

	retval = gb_listen(target_cport);
	if (retval) {
		LOG_ERR("Can not connect cport %d: error %d", sys_le16_to_cpu(req_data->cport_id),
			retval);
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}

	retval = gb_notify(target_cport, GB_EVT_CONNECTED);
	if (retval) {
		goto error_notify;
	}

	return gb_transport_message_empty_response_send(req, GB_OP_SUCCESS, cport);

error_notify:
	gb_stop_listening(cport);
	gb_transport_message_empty_response_send(req, gb_errno_to_op_result(retval), cport);
}

static void gb_control_disconnected(uint16_t cport, struct gb_message *req)
{
	int retval;
	const struct gb_control_disconnected_request *req_data =
		(const struct gb_control_disconnected_request *)req->payload;

	if (gb_message_payload_len(req) < sizeof(*req_data)) {
		LOG_ERR("dropping short message");
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}

	retval = gb_notify(sys_le16_to_cpu(req_data->cport_id), GB_EVT_DISCONNECTED);
	if (retval) {
		LOG_ERR("Cannot notify GB driver of disconnect event.");
		/*
		 * don't return, we still want to reset the cport and stop listening
		 * on the CPort.
		 */
	}

	retval = gb_stop_listening(cport);
	if (retval) {
		LOG_ERR("Can not disconnect cport %d: error %d", cport, retval);
	}

	gb_transport_message_empty_response_send(req, gb_errno_to_op_result(retval), cport);
}

static void gb_control_disconnecting(uint16_t cport, struct gb_message *req)
{
	gb_transport_message_empty_response_send(req, GB_OP_SUCCESS, cport);
}

static void gb_control_pm_stub(uint16_t cport, struct gb_message *req)
{
	const struct gb_control_bundle_pm_response resp_data = {
		.status = GB_CONTROL_BUNDLE_PM_OK,
	};

	gb_transport_message_response_success_send(req, &resp_data, sizeof(resp_data), cport);
}

static void gb_control_handler(const void *priv, struct gb_message *msg, uint16_t cport)
{
	ARG_UNUSED(priv);

	switch (gb_message_type(msg)) {
	case GB_CONTROL_TYPE_VERSION:
		return gb_control_protocol_version(cport, msg);
	case GB_CONTROL_TYPE_GET_MANIFEST_SIZE:
		return gb_control_get_manifest_size(cport, msg);
	case GB_CONTROL_TYPE_GET_MANIFEST:
		return gb_control_get_manifest(cport, msg);
	case GB_CONTROL_TYPE_CONNECTED:
		return gb_control_connected(cport, msg);
	case GB_CONTROL_TYPE_DISCONNECTED:
		return gb_control_disconnected(cport, msg);
	case GB_CONTROL_TYPE_DISCONNECTING:
		return gb_control_disconnecting(cport, msg);
	case GB_CONTROL_TYPE_BUNDLE_ACTIVATE:
	case GB_CONTROL_TYPE_BUNDLE_SUSPEND:
	case GB_CONTROL_TYPE_BUNDLE_RESUME:
	case GB_CONTROL_TYPE_BUNDLE_DEACTIVATE:
	case GB_CONTROL_TYPE_INTF_SUSPEND_PREPARE:
	case GB_CONTROL_TYPE_INTF_DEACTIVATE_PREPARE:
		return gb_control_pm_stub(cport, msg);
	/* XXX SW-4136: see control-gb.h */
	/*GB_HANDLER(GB_CONTROL_TYPE_INTF_POWER_STATE_SET, gb_control_intf_pwr_set),
	GB_HANDLER(GB_CONTROL_TYPE_BUNDLE_POWER_STATE_SET, gb_control_bundle_pwr_set),*/
	/* TODO: Properly implement timesync */
	case GB_CONTROL_TYPE_TIMESYNC_ENABLE:
	case GB_CONTROL_TYPE_TIMESYNC_DISABLE:
	case GB_CONTROL_TYPE_TIMESYNC_AUTHORITATIVE:
	case GB_CONTROL_TYPE_TIMESYNC_GET_LAST_EVENT:
		return gb_transport_message_empty_response_send(msg, GB_OP_SUCCESS, cport);
	default:
		LOG_ERR("Invalid type");
		gb_transport_message_empty_response_send(msg, GB_OP_INVALID, cport);
	}
}

const struct gb_driver gb_control_driver = {
	.op_handler = gb_control_handler,
};
