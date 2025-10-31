/*
 * Copyright (c) 2015 Google, Inc.
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

#include "greybus_transport.h"
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include "greybus_spi.h"
#include "greybus_internal.h"

LOG_MODULE_REGISTER(greybus_spi, CONFIG_GREYBUS_LOG_LEVEL);

/**
 * @brief Returns a set of configuration parameters related to SPI master.
 */
static void gb_spi_protocol_master_config(uint16_t cport, struct gb_message *req,
					  const struct gb_spi_driver_data *data)
{
	ARG_UNUSED(data);

	/* TODO: Zephyr should provide API to get these details */
	const struct gb_spi_master_config_response resp_data = {
		.min_speed_hz = 738,
		.max_speed_hz = 24000000,
		.mode = 0,
		.flags = 0,
		.num_chipselect = 1,
	};

	gb_transport_message_response_success_send(req, &resp_data, sizeof(resp_data), cport);
}

/**
 * @brief Get configuration parameters from chip
 *
 * Returns a set of configuration parameters taht related to SPI device is
 * selected.
 */
static void gb_spi_protocol_device_config(uint16_t cport, struct gb_message *req,
					  const struct gb_spi_driver_data *data)
{
	const struct gb_spi_device_config_request *req_data =
		(const struct gb_spi_device_config_request *)req->payload;
	struct gb_spi_device_config_response dev_data;
	size_t i;

	strncpy(dev_data.name, data->dev->name, sizeof(dev_data.name));

	for (i = 0; i < data->device_num; i++) {
		if (data->device_num == req_data->chip_select) {
			return gb_transport_message_response_success_send(
				req, &data->devices[req_data->chip_select].data, sizeof(dev_data),
				cport);
		}
	}

	/* Use normal spi dev device
	 * TODO: Need to be the same as master config
	 */
	dev_data.max_speed_hz = 24000000;
	dev_data.mode = 0;
	dev_data.bits_per_word = 0;
	dev_data.device_type = GB_SPI_SPI_DEV;

	gb_transport_message_response_success_send(req, &dev_data, sizeof(dev_data), cport);
}

/**
 * @brief Performs a SPI transaction as one or more SPI transfers, defined
 *        in the supplied array.
 */
static void gb_spi_protocol_transfer(uint16_t cport, struct gb_message *req,
				     const struct gb_spi_driver_data *data)
{
	int ret;
	struct gb_spi_transfer_request *req_data = (struct gb_spi_transfer_request *)req->payload;
	struct gb_spi_transfer *desc;
	struct spi_config conf = {0};
	size_t i, resp_pos = 0, resp_size = 0;
	struct gb_message *resp;
	struct spi_buf tx_buf, rx_buf;
	uint8_t *trans_data = req->payload + 4 + (13 * req_data->count);
	const struct spi_buf_set tx_buf_set = {
		.buffers = &tx_buf,
		.count = 1,
	};
	const struct spi_buf_set rx_buf_set = {
		.buffers = &rx_buf,
		.count = 1,
	};

	if (req_data->mode & (GB_SPI_MODE_NO_CS | GB_SPI_MODE_3WIRE | GB_SPI_MODE_READY)) {
		LOG_ERR("SPI Mode %u is not supported", req_data->mode);
		return gb_transport_message_empty_response_send(req, GB_OP_INTERNAL, cport);
	}

	conf.slave = req_data->chip_select;
	if (req_data->mode & GB_SPI_MODE_CPHA) {
		conf.operation |= SPI_MODE_CPHA;
	}
	if (req_data->mode & GB_SPI_MODE_CPOL) {
		conf.operation |= SPI_MODE_CPOL;
	}
	if (req_data->mode & GB_SPI_MODE_CS_HIGH) {
		conf.operation |= SPI_CS_ACTIVE_HIGH;
	}
	if (req_data->mode & GB_SPI_MODE_LSB_FIRST) {
		conf.operation |= SPI_TRANSFER_LSB;
	}
	if (req_data->mode & GB_SPI_MODE_LOOP) {
		conf.operation |= SPI_MODE_LOOP;
	}

	/* Calculate the response size */
	for (i = 0; i < req_data->count; ++i) {
		desc = &req_data->transfers[i];
		if (desc->xfer_flags & GB_SPI_XFER_READ) {
			resp_size += desc->len;
		}
	}

	resp = gb_message_alloc(resp_size, GB_RESPONSE(GB_SPI_TYPE_TRANSFER),
				req->header.operation_id, GB_OP_SUCCESS);
	for (i = 0; i < req_data->count; ++i) {
		desc = &req_data->transfers[i];
		conf.frequency = desc->speed_hz;
		conf.operation |= SPI_WORD_SET(desc->bits_per_word);

		if (desc->cs_change) {
			LOG_ERR("cs_change not supported");
			gb_transport_message_empty_response_send(req, GB_OP_INTERNAL, cport);
			goto free_resp;
		}

		if ((desc->xfer_flags & GB_SPI_XFER_READ) &&
		    (desc->xfer_flags & GB_SPI_XFER_WRITE)) {
			rx_buf.buf = resp->payload + resp_pos;
			rx_buf.len = desc->len;
			tx_buf.buf = trans_data;
			tx_buf.len = desc->len;

			ret = spi_transceive(data->dev, &conf, &tx_buf_set, &rx_buf_set);
			if (ret < 0) {
				LOG_ERR("SPI transceive failed");
				gb_transport_message_empty_response_send(req, GB_OP_INTERNAL,
									 cport);
				goto free_resp;
			}

			trans_data += desc->len;
			resp_pos += desc->len;
		} else if (desc->xfer_flags & GB_SPI_XFER_READ) {
			rx_buf.buf = resp->payload + resp_pos;
			rx_buf.len = desc->len;

			ret = spi_read(data->dev, &conf, &rx_buf_set);
			if (ret < 0) {
				LOG_ERR("SPI read failed");
				gb_transport_message_empty_response_send(req, GB_OP_INTERNAL,
									 cport);
				goto free_resp;
			}

			resp_pos += desc->len;
		} else if (desc->xfer_flags & GB_SPI_XFER_WRITE) {
			tx_buf.buf = trans_data;
			tx_buf.len = desc->len;
			ret = spi_write(data->dev, &conf, &tx_buf_set);
			if (ret < 0) {
				LOG_ERR("SPI write failed");
				gb_transport_message_empty_response_send(req, GB_OP_INTERNAL,
									 cport);
				goto free_resp;
			}

			trans_data += desc->len;
		} else {
			LOG_ERR("Invalid flag");
			gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
			goto free_resp;
		}

		k_sleep(K_USEC(desc->delay_usecs));
	}

	gb_transport_message_send(resp, cport);
	gb_message_dealloc(req);

free_resp:
	gb_message_dealloc(resp);
}

static void gb_spi_handler(const void *priv, struct gb_message *msg, uint16_t cport)
{
	const struct gb_spi_driver_data *data = priv;

	switch (gb_message_type(msg)) {
	case GB_SPI_TYPE_MASTER_CONFIG:
		return gb_spi_protocol_master_config(cport, msg, data);
	case GB_SPI_TYPE_DEVICE_CONFIG:
		return gb_spi_protocol_device_config(cport, msg, data);
	case GB_SPI_TYPE_TRANSFER:
		return gb_spi_protocol_transfer(cport, msg, data);
	default:
		LOG_ERR("Invalid type");
		gb_transport_message_empty_response_send(msg, GB_OP_INVALID, cport);
	}
}

const struct gb_driver gb_spi_driver = {
	.op_handler = gb_spi_handler,
};
