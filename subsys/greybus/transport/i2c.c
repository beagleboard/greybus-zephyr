/*
 * Copyright (c) 2026 BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../greybus_transport.h"
#include <greybus/greybus.h>
#include <zephyr/kernel.h>
#include <greybus-utils/manifest.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include "../greybus_heap.h"
#include <assert.h>

#define PIPE_LEN 256

LOG_MODULE_REGISTER(greybus_transport_i2c, CONFIG_GREYBUS_LOG_LEVEL);

static uint8_t ring_buf_data[PIPE_LEN];
static struct ring_buf rx_buf;

static uint8_t tx_pipe_data[PIPE_LEN];
static struct k_pipe tx_pipe;

static const struct device *bus = DEVICE_DT_GET(DT_ALIAS(greybus_transport));

static void gb_msg_process_cb(struct k_work *work);
static K_WORK_DEFINE(gb_msg_process_work, gb_msg_process_cb);

/**
 * Format for greybus message header in ring buffer. This will optionally be followed by payload.
 */
struct rx_ring_data_header {
	__le16 cport;
	struct gb_operation_msg_hdr hdr;
} __packed;

static int gb_msg_rx_take(struct gb_msg_with_cport *msg)
{
	int ret;
	uint16_t data_len;
	struct rx_ring_data_header hdr;

	if (ring_buf_size_get(&rx_buf) < sizeof(hdr)) {
		return -ENODATA;
	}

	ret = ring_buf_peek(&rx_buf, (uint8_t *)&hdr, sizeof(hdr));
	if (ret < sizeof(hdr)) {
		return -ENODATA;
	}

	data_len = gb_hdr_message_len(&hdr.hdr);
	if (ring_buf_size_get(&rx_buf) < sizeof(uint16_t) + data_len) {
		return -ENODATA;
	}

	msg->msg = gb_alloc(data_len);
	if (!msg->msg) {
		LOG_ERR("Failed to allocate message");
		return -ENOMEM;
	}

	msg->cport = sys_le16_to_cpu(hdr.cport);

	ret = ring_buf_get(&rx_buf, NULL, sizeof(uint16_t));
	__ASSERT(ret == sizeof(scratch), "Failed to get cport");

	ret = ring_buf_get(&rx_buf, (uint8_t *)msg->msg, data_len);
	__ASSERT(ret == data_len, "Failed to get greybus message");

	return 0;
}

static void gb_msg_process_cb(struct k_work *work)
{
	struct gb_msg_with_cport msg;

        ARG_UNUSED(work);

	while (gb_msg_rx_take(&msg) == 0) {
		if (greybus_rx_handler(msg.cport, msg.msg) < 0) {
			LOG_ERR("Failed to handle greybus message");
		}
	}
}

static int i2c_target_write_requested_cb(struct i2c_target_config *config)
{
	return 0;
}

static int i2c_target_write_received_cb(struct i2c_target_config *config, uint8_t val)
{
	int ret;

	ret = ring_buf_put(&rx_buf, &val, sizeof(val));
	if (ret != sizeof(val)) {
		LOG_DBG("Dropping data");
		return -ENOMEM;
	}

	return 0;
}

static int i2c_target_stop_cb(struct i2c_target_config *config)
{
	k_work_submit(&gb_msg_process_work);

	return 0;
}

static int i2c_target_read_cb(struct i2c_target_config *config, uint8_t *val)
{
	int ret;

	ret = k_pipe_read(&tx_pipe, val, sizeof(*val), K_NO_WAIT);
	if (ret != sizeof(*val)) {
		LOG_DBG("Failed to read data");
		return -ENODATA;
	}

	return 0;
}

static const struct i2c_target_callbacks target_cbs = {
	.read_requested = i2c_target_read_cb,
	.read_processed = i2c_target_read_cb,
	.write_requested = i2c_target_write_requested_cb,
	.write_received = i2c_target_write_received_cb,
	.stop = i2c_target_stop_cb,
};

static struct i2c_target_config target_cfg = {
	.address = CONFIG_GREYBUS_XPORT_I2C_ADDRESS,
	.callbacks = &target_cbs,
};

static int gb_trans_init()
{
	int ret;

	if (!device_is_ready(bus)) {
		LOG_ERR("I2C bus not ready");
		return -ENODEV;
	}

	k_pipe_init(&tx_pipe, tx_pipe_data, sizeof(tx_pipe_data));
	ring_buf_init(&rx_buf, ARRAY_SIZE(ring_buf_data), ring_buf_data);

	ret = i2c_target_register(bus, &target_cfg);
	if (ret < 0) {
		LOG_ERR("Failed to register target: %d", ret);
		return ret;
	}

	return 0;
}

static void gb_trans_exit(void)
{
	k_pipe_close(&tx_pipe);
	ring_buf_reset(&rx_buf);

	if (i2c_target_unregister(bus, &target_cfg) < 0) {
		LOG_ERR("Failed to unregister target\n");
	}
}

static int gb_trans_listen(uint16_t cport)
{
        ARG_UNUSED(cport);

	return 0;
}

static int gb_trans_send(uint16_t cport, const struct gb_message *msg)
{
	const __le16 cport_le = sys_cpu_to_le16(cport);
	int ret;

	ret = k_pipe_write(&tx_pipe, (const uint8_t *)&cport_le, sizeof(cport_le), K_FOREVER);
	if (ret != sizeof(cport_le)) {
		return -EIO;
	}

	ret = k_pipe_write(&tx_pipe, (const uint8_t *)msg, gb_message_len(msg), K_FOREVER);
	if (ret != gb_message_len(msg)) {
		return -EIO;
	}

	return 0;
}

const struct gb_transport_backend gb_trans_backend = {
	.init = gb_trans_init,
	.exit = gb_trans_exit,
	.listen = gb_trans_listen,
	.send = gb_trans_send,
};
