/*
 * Copyright (c) 2025 Ayush Singh, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GREYBUS_LOG_H_
#define _GREYBUS_LOG_H_

#include <stdint.h>

/**
 * Send greybus log message
 *
 * @param len: size of string excluding NULL terminator.
 * @param log: UTF-8 string.
 */
void gb_log_send_log(uint16_t len, const char *log);

#endif // _GREYBUS_LOG_H_
