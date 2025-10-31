/*
 * Copyright (c) 2014-2015 Google Inc.
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

#ifndef _GREYBUS_UTILS_MANIFEST_H_
#define _GREYBUS_UTILS_MANIFEST_H_

#include <zephyr/sys/dlist.h>
#include <zephyr/types.h>
#include <zephyr/devicetree.h>

#define _GREYBUS_BASE_NODE DT_PATH(zephyr_greybus)

#define _BUNDLE_PROP_LEN(node_id, cfg, prop)                                                       \
	COND_CODE_1(cfg, (DT_PROP_LEN_OR(node_id, prop, 0)), (0))

#define _GREYBUS_CPORTS_IN_BRIDGED_PHY_BUNDLE(node_id)                                             \
	(_BUNDLE_PROP_LEN(node_id, CONFIG_GREYBUS_GPIO, gpio_controllers) +                        \
	 _BUNDLE_PROP_LEN(node_id, CONFIG_GREYBUS_I2C, i2c_controllers) +                          \
	 _BUNDLE_PROP_LEN(node_id, CONFIG_GREYBUS_SPI, spi_controllers) +                          \
	 _BUNDLE_PROP_LEN(node_id, CONFIG_GREYBUS_UART, uart_controllers) +                        \
	 _BUNDLE_PROP_LEN(node_id, CONFIG_GREYBUS_PWM, pwm_controllers))

#define _GREYBUS_CPORTS_IN_VIBRATOR_BUNDLE(_node_id)                                               \
	_BUNDLE_PROP_LEN(_node_id, CONFIG_GREYBUS_VIBRATOR, vibrators)

#define _GREYBUS_CPORT_COUNTER(_node_id)                                                           \
	COND_CODE_1(DT_NODE_HAS_COMPAT_STATUS(_node_id, zephyr_greybus_bundle_bridged_phy, okay),  \
		    (_GREYBUS_CPORTS_IN_BRIDGED_PHY_BUNDLE(_node_id)),                             \
		    (COND_CODE_1(DT_NODE_HAS_COMPAT_STATUS(_node_id,                               \
							   zephyr_greybus_bundle_vibrator, okay),  \
				 (_GREYBUS_CPORTS_IN_VIBRATOR_BUNDLE(_node_id)), (1))))

/*
 * Handler for cports and bundles that do not exist in DT.
 * - Control
 * - Loopback
 * - Log
 * - Raw
 */
#define _GREYBUS_SPECIAL_CPORTS                                                                    \
	(1 + COND_CODE_1(CONFIG_GREYBUS_LOOPBACK, (1), (0)) +                                      \
	 COND_CODE_1(CONFIG_GREYBUS_FW, (2), (0)) +                                                \
	 COND_CODE_1(CONFIG_GREYBUS_LOG_BACKEND, (1), (0)) + GREYBUS_RAW_CPORT_COUNT)

#define GREYBUS_CPORT_COUNT                                                                        \
	(_GREYBUS_SPECIAL_CPORTS +                                                                 \
	 COND_CODE_0(DT_CHILD_NUM_STATUS_OKAY(_GREYBUS_BASE_NODE), (0),                            \
		     (DT_FOREACH_CHILD_STATUS_OKAY_SEP(_GREYBUS_BASE_NODE, _GREYBUS_CPORT_COUNTER, \
						       (+)))))

#define GREYBUS_FW_MANAGEMENT_CPORT 1
#define GREYBUS_FW_DOWNLOAD_CPORT   2
#define GREYBUS_LOG_CPORT           COND_CODE_1(CONFIG_GREYBUS_FW, (3), (1))
#define GREYBUS_RAW_CPORT_START                                                                    \
	(COND_CODE_1(CONFIG_GREYBUS_FW, (2), (0)) +                                                \
	 COND_CODE_1(CONFIG_GREYBUS_LOG_BACKEND, (1), (0)) + 1)
#define GREYBUS_RAW_CPORT_COUNT COND_CODE_1(CONFIG_GREYBUS_RAW, (CONFIG_GREYBUS_RAW_CPORTS), (0))

typedef void (*manifest_handler)(unsigned char *manifest_file, int device_id, int manifest_number);

/**
 * Write greybus manifest to the buffer. The buffer length should be >= GREYBUS_MANIFEST_SIZE
 *
 * @return size written if successful.
 * @return -errno in case of error.
 */
int manifest_create(uint8_t buf[], size_t len);

/**
 * Get greybus manifest size.
 */
size_t manifest_size(void);

/**
 * Print greybus manifest to stdout. Intended for debugging.
 */
void manifest_print(uint8_t buf[]);

#endif
