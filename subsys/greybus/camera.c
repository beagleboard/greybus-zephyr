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

struct gb_camera_info {
	unsigned int cport;
	struct device *dev;
	uint8_t state;
};

static struct gb_camera_info *info = NULL;

static void gb_camera_protocol_version(uint16_t cport, struct gb_message *msg)
{
    struct gb_camera_version_response response = {
        .major = GB_CAMERA_VERSION_MAJOR,
        .minor = GB_CAMERA_VERSION_MINOR,
    };

    LOG_DBG("gb_camera_protocol_version() called");

    gb_transport_message_response_success_send(msg, &response, sizeof(response), cport);
}

static void gb_camera_capabilities(uint16_t cport, struct gb_message *msg)
{
    struct gb_camera_capabilities_response *response;
    struct video_caps vcaps;
    size_t payload_size;
    size_t total_size;
    int ret;

    LOG_DBG("gb_camera_capabilities() + ");

    if (info == NULL || info->state < STATE_UNCONFIGURED) {
        LOG_ERR("Camera in invalid state");
        gb_transport_message_empty_response_send(msg, GB_OP_INVALID, cport);
        return;
    }

    ret = video_get_caps(info->dev, &vcaps);
    if (ret) {
        LOG_ERR("Failed to get video caps from device");
        gb_transport_message_empty_response_send(msg, GB_OP_UNKNOWN_ERROR, cport);
        return;
    }

    /* to determine the payload size 
     * (For now, we assume the Greybus spec requires a 4-byte dummy payload. 
     *  write the real translation logic kater)
     */
    payload_size = 4; 
    total_size = sizeof(*response) + payload_size;

    response = calloc(1, total_size);
    if (!response) {
        LOG_ERR("Failed to allocate capabilities response");
        gb_transport_message_empty_response_send(msg, GB_OP_NO_MEMORY, cport);
        return;
    }

    response->capabilities[0] = 0xAA; 
    response->capabilities[1] = 0xBB; 
    response->capabilities[2] = 0xCC; 
    response->capabilities[3] = 0xDD; 

    gb_transport_message_response_success_send(msg, response, total_size, cport);

    free(response);

    LOG_DBG("gb_camera_capabilities()- ");
}

static void gb_camera_connected(const void *priv, uint16_t cport)
{
    LOG_DBG("gb_camera_connected + ");

    info = calloc(1, sizeof(*info));
    if (info == NULL) {
        LOG_ERR("Failed to allocate memory");
        return;
    }

    info->cport = cport;
    info->state = STATE_INSERTED;
    info->dev = (struct device *)DEVICE_DT_GET(DT_ALIAS(camera0));
    
    if (!device_is_ready(info->dev)) {
        LOG_ERR("Camera device not ready");
        free(info);
        info = NULL;
        return;
    }

    info->state = STATE_UNCONFIGURED;

    LOG_DBG("gb_camera_connected- ");
}

static void gb_camera_disconnected(const void *priv)
{
    LOG_DBG("gb_camera_disconnected");
    if (info) {
        free(info);
        info = NULL;
    }
}

static void gb_camera_handler(const void *priv, struct gb_message *msg, uint16_t cport)
{
    switch (gb_message_type(msg)) {
    case GB_CAMERA_TYPE_PROTOCOL_VERSION:
        gb_camera_protocol_version(cport, msg);
        break;
    case GB_CAMERA_TYPE_CAPABILITIES:
        gb_camera_capabilities(cport, msg);
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
