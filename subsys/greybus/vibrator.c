/*
 * Copyright (c) 2015 Google Inc.
 * All rights reserved.
 * Author: Eli Sennesh <esennesh@leaflabs.com>
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

#include <zephyr/drivers/haptics.h>
#include <greybus/greybus_protocols.h>
#include <greybus/greybus_messages.h>
#include "greybus_transport.h"
#include "greybus_internal.h"

static void gb_vibrator_vibrator_on(uint16_t cport, struct gb_message *req,
				    const struct device *dev)
{
	int ret = haptics_start_output(dev);

	gb_transport_message_empty_response_send(req, gb_errno_to_op_result(ret), cport);
}

static void gb_vibrator_vibrator_off(uint16_t cport, struct gb_message *req,
				     const struct device *dev)
{
	int ret = haptics_stop_output(dev);

	gb_transport_message_empty_response_send(req, gb_errno_to_op_result(ret), cport);
}

static void gb_vibrator_handler(const void *priv, struct gb_message *msg, uint16_t cport)
{
	const struct device *dev = priv;

	switch (gb_message_type(msg)) {
	case GB_VIBRATOR_TYPE_ON:
		return gb_vibrator_vibrator_on(cport, msg, dev);
	case GB_VIBRATOR_TYPE_OFF:
		return gb_vibrator_vibrator_off(cport, msg, dev);
	default:
		gb_transport_message_empty_response_send(msg, GB_OP_INVALID, cport);
	}
}

const struct gb_driver gb_vibrator_driver = {
	.op_handler = gb_vibrator_handler,
};
