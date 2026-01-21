/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * In a Greybus network, the component responsible for creating connections
 * and routing messages between greybus host and greybus node is known as
 * APBridge.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GREYBUS_APBRIDGE_H_
#define _GREYBUS_APBRIDGE_H_

#include <greybus/greybus_messages.h>

#define AP_MAX_NODES  CONFIG_GREYBUS_APBRIDGE_CPORTS
#define SVC_INF_ID    0
#define AP_INF_ID     1
#define INTF_START_ID 2

struct gb_interface;

/**
 * Callback for writing to an interface
 *
 * @param controller
 * @param greybus message to send
 * @param Cport to write to
 *
 * @return 0 if successful. Negative in case of error
 */
typedef int (*gb_controller_write_callback_t)(struct gb_interface *, struct gb_message *, uint16_t);

/**
 * Callback to create new connection with a Cport in the interface
 *
 * @param controller
 * @param cport
 *
 * @return 0 if successful. Negative in case of error
 */
typedef int (*gb_controller_create_connection_t)(struct gb_interface *, uint16_t);

/**
 * Callback to destroy connection with a Cport in the interface
 *
 * @param controller
 * @param cport
 */
typedef void (*gb_controller_destroy_connection_t)(struct gb_interface *, uint16_t);

/**
 * A greybus interface. Can have multiple Cports
 *
 * @param id: Interface ID
 * @param write: a non-blocking write function. The ownership of message is
 * transferred.
 * @param create_connection: Called when a new connection with a cport is created. Optional.
 * @param destroy_connection: Called when an existing connection with a cport is destroyed.
 * Optional.
 * @param ctrl_data: private controller data
 */
struct gb_interface {
	gb_controller_write_callback_t write;
	gb_controller_create_connection_t create_connection;
	gb_controller_destroy_connection_t destroy_connection;
	void *ctrl_data;
	uint8_t id;
};

/**
 * Initialize and start APBridge
 *
 * @return 0 in case of success.
 * @return < 0 in case of error.
 */
int gb_apbridge_init(void);

/*
 * De-Initialize and stop APBridge.
 */
void gb_apbridge_deinit(void);

/**
 * Create connection between 2 interface cports.
 *
 * NOTE: It is only possible to create connection between AP-Node. Node-Node and AP-AP are not
 * supported.
 *
 * @param intf1_id
 * @param intf1_cport
 * @param intf2_id
 * @param intf2_cport
 *
 * @return 0 in case of success.
 * @return < 0 in case of error.
 */
int gb_apbridge_connection_create(uint8_t intf1_id, uint16_t intf1_cport, uint8_t intf2_id,
				  uint16_t intf2_cport);

/**
 * Destroy connection between 2 interface cports.
 *
 * @param intf1_id
 * @param intf1_cport
 * @param intf2_id
 * @param intf2_cport
 *
 * @return 0 in case of success.
 * @return < 0 in case of error.
 */
int gb_apbridge_connection_destroy(uint8_t intf1_id, uint16_t intf1_cport, uint8_t intf2_id,
				   uint16_t intf2_cport);

/**
 * Send message between connected cports.
 *
 * Looks up the target connected inteface and sends the greybus message.
 *
 * @param intf_id: Interface ID of the origin.
 * @param intf_cport: Interface CPort of the origin.
 * @param msg: Message to send
 *
 * @return 0 in case of success.
 * @return < 0 in case of error.
 */
int gb_apbridge_send(uint8_t intf_id, uint16_t intf_cport, struct gb_message *msg);

/**
 * Get greybus interface by ID;
 */
struct gb_interface *gb_interface_get(uint8_t id);

/**
 * Allocate greybus interface dynamically.
 *
 * Also adds interface to cache.
 *
 * @param write_cb
 * @param create_connection_cb
 * @param destroy_connection_cb
 * @param ctrl_data
 *
 * @return NULL in case of error
 */
struct gb_interface *gb_interface_alloc(gb_controller_write_callback_t write_cb,
					gb_controller_create_connection_t create_connection_cb,
					gb_controller_destroy_connection_t destroy_connection_cb,
					void *ctrl_data);

/**
 * De-allocate greybus interface
 *
 * Also removes greybus interface from cache.
 *
 * @param intf
 */
void gb_interface_dealloc(struct gb_interface *intf);

/**
 * Add greybus interface to cache.
 *
 * @param intf
 *
 * @return 0 in case of success.
 * @return < 0 in case of error.
 */
int gb_interface_add(struct gb_interface *intf);

/**
 * Remove greybus interface from cache
 *
 * @param id: Greybus interface ID
 */
void gb_interface_remove(uint8_t id);

#endif // _GREYBUS_APBRIDGE_H_
