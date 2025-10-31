/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GREYBUS_FW_MGMT_H_
#define _GREYBUS_FW_MGMT_H_

#include <stdint.h>

extern const struct gb_driver gb_fw_mgmt_driver;

void gb_fw_mgmt_interface_fw_loaded(uint8_t id, uint8_t status, uint16_t major, uint16_t minor);

#endif // _GREYBUS_FW_DOWNLOAD_H_
