/*
 * Copyright (c) 2014-2015 Google Inc.
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
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
 */

#include <zephyr/drivers/i2c.h>
#include <greybus/greybus_protocols.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>
#include "greybus_transport.h"
#include "greybus_internal.h"

LOG_MODULE_REGISTER(greybus_i2c, CONFIG_GREYBUS_LOG_LEVEL);

static void gb_i2c_protocol_functionality(uint16_t cport, struct gb_message *req)
{
	const struct gb_i2c_functionality_response resp_data = {
		.functionality =
			GB_I2C_FUNC_I2C | GB_I2C_FUNC_SMBUS_READ_BYTE |
			GB_I2C_FUNC_SMBUS_WRITE_BYTE | GB_I2C_FUNC_SMBUS_READ_BYTE_DATA |
			GB_I2C_FUNC_SMBUS_WRITE_BYTE_DATA | GB_I2C_FUNC_SMBUS_READ_WORD_DATA |
			GB_I2C_FUNC_SMBUS_WRITE_WORD_DATA | GB_I2C_FUNC_SMBUS_READ_I2C_BLOCK |
			GB_I2C_FUNC_SMBUS_WRITE_I2C_BLOCK,
	};

	gb_transport_message_response_success_send(req, &resp_data, sizeof(resp_data), cport);
}

static void gb_i2c_protocol_transfer(uint16_t cport, struct gb_message *req,
				     const struct device *dev)
{
	const struct gb_i2c_transfer_op *desc;
	const uint8_t *write_data;
	uint8_t *read_data;
	const struct gb_i2c_transfer_request *req_data =
		(const struct gb_i2c_transfer_request *)req->payload;
	struct gb_message *resp;
	size_t i, resp_size = 0;
	uint16_t op_size, addr, op_count;
	int ret;

	op_count = sys_le16_to_cpu(req_data->op_count);
	write_data = (const uint8_t *)&req_data->ops[op_count];

	for (i = 0; i < op_count; i++) {
		desc = &req_data->ops[i];
		if (desc->flags & GB_I2C_M_RD) {
			resp_size += sys_le16_to_cpu(desc->size);
		}
	}

	resp = gb_message_alloc(resp_size, GB_RESPONSE(req->header.type), req->header.operation_id,
				GB_OP_SUCCESS);
	if (!resp) {
		LOG_ERR("Failed to allocate response");
		return gb_transport_message_empty_response_send(req, GB_OP_NO_MEMORY, cport);
	}
	read_data = resp->payload;

	for (i = 0; i < op_count; i++) {
		desc = &req_data->ops[i];
		op_size = sys_le16_to_cpu(desc->size);
		addr = sys_le16_to_cpu(desc->addr);

		if (desc->flags & GB_I2C_M_RD) {
			ret = i2c_read(dev, read_data, op_size, desc->addr);
			if (ret < 0) {
				LOG_ERR("Failed to read i2c data");
				ret = gb_errno_to_op_result(ret);
				goto free_msg;
			}
			read_data += op_size;
		} else {
			ret = i2c_write(dev, write_data, op_size, desc->addr);
			if (ret < 0) {
				LOG_ERR("Failed to write i2c data");
				ret = gb_errno_to_op_result(ret);
				goto free_msg;
			}
			write_data += op_size;
		}
	}

	gb_transport_message_send(resp, cport);
	return gb_message_dealloc(resp);

free_msg:
	gb_message_dealloc(resp);
	return gb_transport_message_empty_response_send(req, ret, cport);
}

static void gb_i2c_handler(const void *priv, struct gb_message *msg, uint16_t cport)
{
	const struct device *dev = priv;

	switch (gb_message_type(msg)) {
	case GB_I2C_TYPE_FUNCTIONALITY:
		return gb_i2c_protocol_functionality(cport, msg);
	case GB_I2C_TYPE_TRANSFER:
		return gb_i2c_protocol_transfer(cport, msg, dev);
	default:
		LOG_ERR("Invalid type");
		gb_transport_message_empty_response_send(msg, GB_OP_INVALID, cport);
	}
}

const struct gb_driver gb_i2c_driver = {
	.op_handler = gb_i2c_handler,
};
