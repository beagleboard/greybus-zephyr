/**
 * Copyright (c) 2015 Google, Inc.
 * Copyright (c) 2026 Pavithra C.P, BeagleBoard.org
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

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/video.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include "greybus_heap.h"
#include <greybus/greybus.h>
#include <greybus/greybus_protocols.h>
#include "greybus_transport.h"
#include "greybus_internal.h"
#include "greybus_camera.h"

LOG_MODULE_REGISTER(greybus_camera, CONFIG_GREYBUS_LOG_LEVEL);

#define GB_CAMERA_VERSION_MAJOR 0
#define GB_CAMERA_VERSION_MINOR 1

#define GB_CAM_OP_INVALID_STATE 0x80

#define GB_CAM_DT_NOT_USED 0x00

#define STATE_REMOVED      0
#define STATE_INSERTED     1
#define STATE_UNCONFIGURED 2
#define STATE_CONFIGURED   3
#define STATE_STREAMING    4

/**
 * @brief Handler for the protocol version operation
 * returns the supported Greubus Camera Class major and minor version numbers to the host.
 * @param cport- The CPort number
 * @param msg- The incoming greybus message request
 */
static void gb_camera_protocol_version(uint16_t cport, struct gb_message *msg)
{
	struct gb_camera_version_response response = {
		.major = GB_CAMERA_VERSION_MAJOR,
		.minor = GB_CAMERA_VERSION_MINOR,
	};

	gb_transport_message_response_success_send(msg, &response, sizeof(response), cport);
}

/**
 * @brief Handler for the capabilities operation.
 * This queries the underlying zephyr video device for supported formats and the resolution, then
 * translates them into the Greybus ExtCSI payload format, and dynamically allocates the response
 * buffer to send back
 *
 * @note Currently, this translation explicitly maps only the JPEG format (VIDEO_PX_FMT_JPEG) and
 * stubs the framerate to a default 30fps. Support for mapping additional Zephyr pixel formats to
 * greybus formats should be expanded here in the future.
 *
 * @param data- Pointer to camera driver data
 * @param cport- The CPort number
 * @param msg- Incoming greybus message request
 */
static void gb_camera_capabilities(uint16_t cport, struct gb_message *msg,
				   const struct gb_camera_driver_data *data)
{
	struct gb_camera_capabilities_response *response;
	struct video_caps vcaps = {0};
	struct gb_camera_csi_params *csi_header;
	struct gb_camera_format_desc *format;
	size_t payload_size;
	size_t total_size;
	uint8_t num_formats = 0;
	int ret, i;

	if (data == NULL || data->info.state < STATE_UNCONFIGURED) {
		gb_transport_message_empty_response_send(msg, GB_OP_INVALID, cport);
		return;
	}

	ret = video_get_caps(data->dev, &vcaps);
	if (ret) {
		gb_transport_message_empty_response_send(msg, GB_OP_UNKNOWN_ERROR, cport);
		return;
	}

	while (vcaps.format_caps[num_formats].pixelformat != 0) {
		num_formats++;
	}
	if (num_formats == 0) {
		LOG_ERR("No video formats found from device");
		gb_transport_message_empty_response_send(msg, GB_OP_UNKNOWN_ERROR, cport);
		return;
	}

	payload_size = sizeof(struct gb_camera_csi_params) +
		       (num_formats * sizeof(struct gb_camera_format_desc));
	total_size = sizeof(*response) + payload_size;

	response = gb_alloc(total_size);
	if (!response) {
		gb_transport_message_empty_response_send(msg, GB_OP_NO_MEMORY, cport);
		return;
	}
	memset(response, 0, total_size);

	csi_header = (struct gb_camera_csi_params *)&response->capabilities[0];
	format = (struct gb_camera_format_desc *)&response
			 ->capabilities[sizeof(struct gb_camera_csi_params)];

	csi_header->num_formats = num_formats;

	for (i = 0; i < num_formats; i++) {
		/* Map Pixel Format (Zephyr V4L2-style macro -> Greybus macro) */
		if (vcaps.format_caps[i].pixelformat == VIDEO_PIX_FMT_JPEG) {
			format[i].format = sys_cpu_to_le32(GB_CAMERA_CAP_FMT_JPEG);
		} else {
			format[i].format = sys_cpu_to_le32(0);
			LOG_WRN("Unsupported pixel format found during translation");
		}

		format[i].width = sys_cpu_to_le16(vcaps.format_caps[i].width_max);
		format[i].height = sys_cpu_to_le16(vcaps.format_caps[i].height_max);
		format[i].fps = sys_cpu_to_le16(30); // safe default
	}

	gb_transport_message_response_success_send(msg, response, total_size, cport);

	gb_free(response);
}

/**
 * @brief Callback invoked when the greybus host connects to the cemra cport
 * Initializes camera protocol state and binds the Zephyr video device.
 * @param priv- Private driver data pointer
 * @param cport= The CPort number that was connected.
 */
static void gb_camera_connected(const void *priv, uint16_t cport)
{
	struct gb_camera_driver_data *data = (struct gb_camera_driver_data *)priv;

	if (!data) {
		LOG_ERR("No driver data provided in priv!");
		return;
	}

	memset(&data->info, 0, sizeof(data->info));

	if (!device_is_ready(data->dev)) {
		LOG_ERR("Camera device not ready");
		return;
	}

	data->info.state = STATE_UNCONFIGURED;
}

/**
 * @brief Callbak invoked when the greybus host disconnects.
 * Frees memory resources and also resets the camera states
 * @param priv- Private driver data pointer
 */
static void gb_camera_disconnected(const void *priv)
{
	struct gb_camera_driver_data *data = (struct gb_camera_driver_data *)priv;

	if (data) {
		data->info.state = STATE_REMOVED;
	}
}

/**
 * @brief Main operation handler for the Greybus camera class
 * Routes the inoming greybus messages to their specific handlers based on the operation type.
 * @param priv- Private driver data pointer
 * @param msg- Incoming greybus message request
 * @param cport- The CPort number
 */
static void gb_camera_handler(const void *priv, struct gb_message *msg, uint16_t cport)
{
	const struct gb_camera_driver_data *data = priv;

	switch (gb_message_type(msg)) {
	case GB_CAMERA_TYPE_PROTOCOL_VERSION:
		gb_camera_protocol_version(cport, msg);
		break;
	case GB_CAMERA_TYPE_CAPABILITIES:
		gb_camera_capabilities(cport, msg, data);
		break;
	default:
		LOG_ERR("Invalid type: %d", gb_message_type(msg));
		gb_transport_message_empty_response_send(msg, GB_OP_INVALID, cport);
		break;
	}
}

const struct gb_driver gb_camera_driver = {
	.connected = gb_camera_connected,
	.disconnected = gb_camera_disconnected,
	.op_handler = gb_camera_handler,
};
