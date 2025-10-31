/*
 * Copyright (c) 2025 Ayush Singh, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "greybus_transport.h"
#include <greybus/greybus_protocols.h>
#include <zephyr/dfu/mcuboot.h>
#include <greybus-utils/manifest.h>
#include <zephyr/logging/log.h>
#include "greybus_fw_download.h"
#include "greybus_internal.h"

LOG_MODULE_REGISTER(greybus_fw_mgmt, CONFIG_GREYBUS_LOG_LEVEL);

static void fw_mgmt_interface_fw_version(uint16_t cport, struct gb_message *req)
{
	int ret;
	struct mcuboot_img_header hdr;
	uint8_t active_slot = boot_fetch_active_slot();
	struct gb_fw_mgmt_interface_fw_version_response resp_data;

	ret = boot_read_bank_header(active_slot, &hdr, sizeof(hdr));
	if (ret < 0) {
		return gb_transport_message_empty_response_send(req, GB_OP_INTERNAL, cport);
	}

	resp_data.major = hdr.h.v1.sem_ver.major;
	resp_data.minor = hdr.h.v1.sem_ver.minor;
	strncpy(resp_data.firmware_tag, "gb_bcf", sizeof(resp_data.firmware_tag));

	gb_transport_message_response_success_send(req, &resp_data, sizeof(resp_data), cport);
}

static void fw_mgmt_interface_fw_load_and_validate(uint16_t cport, struct gb_message *req)
{
	uint8_t req_id;
	char firmware_tag[10];
	const struct gb_fw_mgmt_load_and_validate_fw_request *req_data =
		(const struct gb_fw_mgmt_load_and_validate_fw_request *)req->payload;

	if (req_data->load_method != GB_FW_LOAD_METHOD_UNIPRO) {
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}

	req_id = req_data->request_id;
	memcpy(firmware_tag, req_data->firmware_tag, sizeof(firmware_tag));

	gb_transport_message_empty_response_send(req, GB_OP_SUCCESS, cport);
	gb_fw_download_find_firmware(req_id, firmware_tag);
}

static void op_handler(const void *priv, struct gb_message *msg, uint16_t cport)
{
	ARG_UNUSED(priv);

	switch (gb_message_type(msg)) {
	case GB_FW_MGMT_TYPE_INTERFACE_FW_VERSION:
		return fw_mgmt_interface_fw_version(cport, msg);
	case GB_FW_MGMT_TYPE_LOAD_AND_VALIDATE_FW:
		return fw_mgmt_interface_fw_load_and_validate(cport, msg);
	case GB_RESPONSE(GB_FW_MGMT_TYPE_LOADED_FW):
		return gb_message_dealloc(msg);
	default:
		return gb_transport_message_empty_response_send(msg, GB_OP_PROTOCOL_BAD, cport);
	}
}

const struct gb_driver gb_fw_mgmt_driver = {
	.op_handler = op_handler,
};

void gb_fw_mgmt_interface_fw_loaded(uint8_t id, uint8_t status, uint16_t major, uint16_t minor)
{
	struct gb_message *msg = gb_message_request_alloc(
		sizeof(struct gb_fw_mgmt_loaded_fw_request), GB_FW_MGMT_TYPE_LOADED_FW, false);
	struct gb_fw_mgmt_loaded_fw_request *req_data =
		(struct gb_fw_mgmt_loaded_fw_request *)msg->payload;

	req_data->request_id = id;
	req_data->status = status;
	req_data->major = sys_cpu_to_le16(major);
	req_data->minor = sys_cpu_to_le16(minor);

	gb_transport_message_send(msg, GREYBUS_FW_MANAGEMENT_CPORT);
	gb_message_dealloc(msg);
}
