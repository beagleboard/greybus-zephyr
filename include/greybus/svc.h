/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GREYBUS_SVC_H_
#define _GREYBUS_SVC_H_

#include "greybus_messages.h"

/**
 * Intialize SVC
 */
int gb_svc_init(void);

/**
 * De-initialize SVC
 */
void gb_svc_deinit(void);

/**
 * Create SVC_TYPE_VERSION greybus message and queue it for sending.
 *
 * @return 0 if successful, else error.
 */
int gb_svc_send_version(void);

/**
 * Send the SVC module inserted request.
 *
 * @param primary_intf_id: Primary interface id of the new module
 * @param intf_count: Number of interfaces covered by module
 * @param flags
 *
 * @return 0 if successfully, negative in case of error
 */
int gb_svc_send_module_inserted(uint8_t primary_intf_id, uint8_t intf_count, uint16_t flags);

/**
 * Send the SVC module removed request.
 *
 * @param interface id of the module removed
 *
 * @return 0 if successfully, negative in case of error
 */
int gb_svc_send_module_removed(uint8_t primary_intf_id);

#endif // _GREYBUS_SVC_H_
