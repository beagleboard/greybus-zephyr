/*
 * Copyright (c) 2025 Ayush Singh, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "greybus_transport.h"
#include <greybus/greybus_protocols.h>
#include <zephyr/dfu/flash_img.h>
#include <zephyr/dfu/mcuboot.h>
#include "greybus_fw_mgmt.h"
#include <greybus-utils/manifest.h>
#include <zephyr/logging/log.h>
#include "greybus_internal.h"

LOG_MODULE_REGISTER(greybus_fw_download, CONFIG_GREYBUS_LOG_LEVEL);

#define DATA_SIZE_MAX 64

struct fw_download_priv_data {
	struct flash_img_context ctx;
	uint32_t fw_size;
	uint32_t offset;
	int req_id;
	uint8_t fw_id;
};

static struct fw_download_priv_data priv_data = {.req_id = -1};

struct fw_fetch_req {
	struct gb_operation_msg_hdr hdr;
	struct gb_fw_download_fetch_firmware_request req;
};

static void gb_fw_download_fetch_firmware(uint16_t cport, uint8_t id, uint16_t offset,
					  uint16_t size)
{
	const struct fw_fetch_req req = {
		.hdr =
			{
				.size = sizeof(req),
				.operation_id = new_operation_id(),
				.type = GB_FW_DOWNLOAD_TYPE_FETCH_FIRMWARE,
				.result = 0,
				.pad = {0, 0},
			},
		.req =
			{
				.firmware_id = id,
				.size = sys_cpu_to_le32(size),
				.offset = sys_cpu_to_le32(offset),
			},
	};

	gb_transport_message_send((const struct gb_message *)&req, cport);
}

static void gb_fw_download_find_firmware_response_handler(uint16_t cport, struct gb_message *resp)
{
	const struct gb_fw_download_find_firmware_response *resp_data =
		(const struct gb_fw_download_find_firmware_response *)resp->payload;

	if (!gb_message_is_success(resp)) {
		LOG_ERR("Find firmware request failed");
		return gb_message_dealloc(resp);
	}

	flash_img_init(&priv_data.ctx);
	priv_data.fw_id = resp_data->firmware_id;
	priv_data.fw_size = resp_data->size;
	priv_data.offset = 0;

	gb_message_dealloc(resp);

	gb_fw_download_fetch_firmware(cport, resp_data->firmware_id, 0, DATA_SIZE_MAX);
}

static void gb_fw_release_firmware(uint16_t cport, u8 firmware_id)
{
	struct gb_message *req =
		gb_message_request_alloc(sizeof(struct gb_fw_download_release_firmware_request),
					 GB_FW_DOWNLOAD_TYPE_RELEASE_FIRMWARE, false);
	struct gb_fw_download_release_firmware_request *req_data =
		(struct gb_fw_download_release_firmware_request *)req->payload;

	req_data->firmware_id = firmware_id;

	gb_transport_message_send(req, cport);
	gb_message_dealloc(req);
}

static void gb_fw_donwnload_early_fail(uint16_t cport, u8 firmware_id, uint8_t req_id)
{
	gb_fw_release_firmware(cport, firmware_id);
	gb_fw_mgmt_interface_fw_loaded(req_id, GB_FW_LOAD_STATUS_FAILED, 0, 0);
}

static void gb_fw_download_fetch_final(uint16_t cport, u8 firmware_id, uint8_t req_id)
{
	int ret;
	struct mcuboot_img_header hdr;

	ret = boot_read_bank_header(1, &hdr, sizeof(hdr));
	if (ret < 0) {
		LOG_ERR("Failed to read new image header");
		return gb_fw_mgmt_interface_fw_loaded(req_id, GB_FW_LOAD_STATUS_FAILED, 0, 0);
	}

	ret = boot_request_upgrade(BOOT_UPGRADE_PERMANENT);
	if (ret < 0) {
		LOG_ERR("Failed to request upgrade: %d", ret);
		return gb_fw_mgmt_interface_fw_loaded(req_id, GB_FW_LOAD_STATUS_FAILED, 0, 0);
	}

	gb_fw_mgmt_interface_fw_loaded(req_id, GB_FW_LOAD_STATUS_VALIDATED, hdr.h.v1.sem_ver.major,
				       hdr.h.v1.sem_ver.minor);
}

static void gb_fw_download_fetch_firmware_response_handler(uint16_t cport, struct gb_message *resp)
{
	int ret;
	uint32_t new_data_size;
	uint32_t cur_data_size = MIN(priv_data.fw_size - priv_data.offset, DATA_SIZE_MAX);
	bool is_final_write = priv_data.offset + cur_data_size >= priv_data.fw_size;

	if (!gb_message_is_success(resp)) {
		LOG_ERR("Fetch firmware request failed");
		return gb_message_dealloc(resp);
	}

	priv_data.offset += cur_data_size;
	if (is_final_write) {
		gb_fw_release_firmware(cport, priv_data.fw_id);
	} else {
		new_data_size = MIN(priv_data.fw_size - priv_data.offset, DATA_SIZE_MAX);
		gb_fw_download_fetch_firmware(cport, priv_data.fw_id, priv_data.offset,
					      new_data_size);
	}

	ret = flash_img_buffered_write(&priv_data.ctx, resp->payload, cur_data_size,
				       is_final_write);
	if (ret < 0) {
		LOG_ERR("Failed to write firmware to flash: %d", ret);
		gb_fw_donwnload_early_fail(cport, priv_data.fw_id, priv_data.fw_size);
		return gb_message_dealloc(resp);
	}

	gb_message_dealloc(resp);

	if (is_final_write) {
		gb_fw_download_fetch_final(cport, priv_data.fw_id, priv_data.req_id);
		priv_data.req_id = -1;
	}

	LOG_INF("Offset: %u", priv_data.offset);
}

static void op_handler(const void *priv, struct gb_message *msg, uint16_t cport)
{
	ARG_UNUSED(priv);

	switch (gb_message_type(msg)) {
	case GB_RESPONSE(GB_FW_DOWNLOAD_TYPE_FIND_FIRMWARE):
		return gb_fw_download_find_firmware_response_handler(cport, msg);
	case GB_RESPONSE(GB_FW_DOWNLOAD_TYPE_FETCH_FIRMWARE):
		return gb_fw_download_fetch_firmware_response_handler(cport, msg);
	case GB_RESPONSE(GB_FW_DOWNLOAD_TYPE_RELEASE_FIRMWARE):
		return gb_message_dealloc(msg);
	default:
		return gb_transport_message_empty_response_send(msg, GB_OP_PROTOCOL_BAD, cport);
	}
}

const struct gb_driver gb_fw_download_driver = {
	.op_handler = op_handler,
};

void gb_fw_download_find_firmware(uint8_t req_id, const char *firmware_tag)
{
	struct gb_message *req =
		gb_message_request_alloc(sizeof(struct gb_fw_download_find_firmware_request),
					 GB_FW_DOWNLOAD_TYPE_FIND_FIRMWARE, false);
	struct gb_fw_download_find_firmware_request *req_data =
		(struct gb_fw_download_find_firmware_request *)req->payload;

	priv_data.req_id = req_id;
	strncpy(req_data->firmware_tag, firmware_tag, sizeof(req_data->firmware_tag));

	gb_transport_message_send(req, GREYBUS_FW_DOWNLOAD_CPORT);
	gb_message_dealloc(req);
}
