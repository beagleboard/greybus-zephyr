/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GREYBUS_FW_DOWNLOAD_H_
#define _GREYBUS_FW_DOWNLOAD_H_

#include <stdint.h>

extern const struct gb_driver gb_fw_download_driver;

void gb_fw_download_find_firmware(uint8_t req_id, const char *firmware_tag);

#endif // _GREYBUS_FW_DOWNLOAD_H_
