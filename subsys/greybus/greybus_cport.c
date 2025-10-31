/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "greybus_cport.h"
#include "greybus-utils/manifest.h"
#include "greybus-manifest.h"
#include <zephyr/logging/log.h>
#include "greybus_gpio.h"
#include "greybus_lights.h"
#include "greybus_pwm.h"
#include "greybus_spi.h"
#include "greybus_uart.h"
#include "greybus_fw_download.h"
#include "greybus_fw_mgmt.h"
#include "greybus_internal.h"
#include "greybus_raw_internal.h"

LOG_MODULE_REGISTER(greybus_cport, CONFIG_GREYBUS_LOG_LEVEL);

extern const struct gb_driver gb_control_driver;
extern const struct gb_driver gb_i2c_driver;
extern const struct gb_driver gb_loopback_driver;
extern const struct gb_driver gb_log_driver;
extern const struct gb_driver gb_vibrator_driver;

/* Reset the counter to 0 */
enum {
	COUNTER_BASE = __COUNTER__
};
#define LOCAL_COUNTER (__COUNTER__ - COUNTER_BASE - 1)

/* Macro to check if a type of cport is present */
#define GB_BRIDGED_PHY_CHECK(_node_id, _ctrl, _config)                                             \
	UTIL_AND(DT_NODE_HAS_PROP(_node_id, _ctrl), _config)

#define GB_GPIO_PRIV_DATA(_node_id, _prop, _idx)                                                   \
	static struct gb_gpio_driver_data gb_gpio_priv_data_##_idx = {                             \
		.dev = DEVICE_DT_GET(DT_PHANDLE_BY_IDX(_node_id, _prop, _idx)),                    \
		.ngpios = DT_PROP(DT_PHANDLE_BY_IDX(_node_id, _prop, _idx), ngpios),               \
	};

#define GB_PWM_PRIV_DATA(_node_id, _prop, _idx)                                                    \
	static struct gb_pwm_channel_data gb_pwm_channel_data_arr_##_idx[1];                       \
	static struct gb_pwm_driver_data gb_pwm_priv_data_##_idx = {                               \
		.channel_data = gb_pwm_channel_data_arr_##_idx,                                    \
		.dev = DEVICE_DT_GET(DT_PHANDLE_BY_IDX(_node_id, _prop, _idx)),                    \
		.channel_num = ARRAY_SIZE(gb_pwm_channel_data_arr_##_idx),                         \
	};

#define GB_SPI_PRIV_DATA(_node_id, _prop, _idx)                                                    \
	static struct gb_spi_driver_data gb_spi_priv_data_##_idx = {                               \
		.dev = DEVICE_DT_GET(DT_PHANDLE_BY_IDX(_node_id, _prop, _idx)),                    \
		.devices = NULL,                                                                   \
		.device_num = 0,                                                                   \
	};

#define GB_BRIDGED_PHY_PRIV_DATA_HANDLER(_node_id)                                                 \
	IF_ENABLED(GB_BRIDGED_PHY_CHECK(_node_id, gpio_controllers, CONFIG_GREYBUS_GPIO),          \
		   (DT_FOREACH_PROP_ELEM(_node_id, gpio_controllers, GB_GPIO_PRIV_DATA)))          \
	IF_ENABLED(GB_BRIDGED_PHY_CHECK(_node_id, spi_controllers, CONFIG_GREYBUS_SPI),            \
		   (DT_FOREACH_PROP_ELEM(_node_id, spi_controllers, GB_SPI_PRIV_DATA)))            \
	IF_ENABLED(GB_BRIDGED_PHY_CHECK(_node_id, pwm_controllers, CONFIG_GREYBUS_PWM),            \
		   (DT_FOREACH_PROP_ELEM(_node_id, pwm_controllers, GB_PWM_PRIV_DATA)))

#define GB_LIGHTS_PRIV_DATA_ITEM(_node_id, _prop, _idx)                                            \
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(_node_id, _prop, _idx))

#define GB_LIGHTS_PRIV_DATA(_node_id)                                                              \
	static const struct device *gb_lights_priv_data_devs[] = {                                 \
		DT_FOREACH_PROP_ELEM_SEP(_node_id, lights, GB_LIGHTS_PRIV_DATA_ITEM, (, ))};       \
	static const struct gb_lights_driver_data gb_lights_priv_data = {                          \
		.lights_num = ARRAY_SIZE(gb_lights_priv_data_devs),                                \
		.devs = gb_lights_priv_data_devs,                                                  \
	};

#define GB_LIGHTS_PRIV_DATA_HANDLER(_node_id)                                                      \
	IF_ENABLED(CONFIG_GREYBUS_LIGHTS, (GB_LIGHTS_PRIV_DATA(_node_id)))

#define GB_PRIV_DATA_HANDLER(_node_id)                                                             \
	COND_CODE_1(DT_NODE_HAS_COMPAT_STATUS(_node_id, zephyr_greybus_bundle_bridged_phy, okay),  \
		    (GB_BRIDGED_PHY_PRIV_DATA_HANDLER(_node_id)),                                  \
		    (IF_ENABLED(DT_NODE_HAS_COMPAT_STATUS(_node_id, zephyr_greybus_bundle_lights,  \
							  okay),                                   \
				(GB_LIGHTS_PRIV_DATA_HANDLER(_node_id)))))

/* Define GPIO private data */
DT_FOREACH_CHILD_STATUS_OKAY(_GREYBUS_BASE_NODE, GB_PRIV_DATA_HANDLER)

#define GB_CPORT_DEV_PRIV_DATA(_node_id, _prop, _idx)                                              \
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(_node_id, _prop, _idx))

#define GB_CPORT_GPIO_PRIV_DATA(_node_id, _prop, _idx) &gb_gpio_priv_data_##_idx

#define GB_CPORT_PWM_PRIV_DATA(_node_id, _prop, _idx) &gb_pwm_priv_data_##_idx

#define GB_CPORT_SPI_PRIV_DATA(_node_id, _prop, _idx) &gb_spi_priv_data_##_idx

#define GB_CPORT(_priv, _bundle, _protocol, _driver)                                               \
	{                                                                                          \
		.bundle = _bundle,                                                                 \
		.protocol = _protocol,                                                             \
		.priv = _priv,                                                                     \
		.driver = _driver,                                                                 \
	}

#define _GB_CPORT(_node_id, _prop, _idx, _bundle, _protocol, _driver, PRIV_FN)                     \
	GB_CPORT(PRIV_FN(_node_id, _prop, _idx), _bundle, _protocol, _driver)

#define GREYBUS_CPORTS_IN_BRIDGED_PHY_BUNDLE(_node_id, _bundle)                                    \
	FOR_EACH_NONEMPTY_TERM(                                                                    \
		IDENTITY, (, ),                                                                    \
		IF_ENABLED(CONFIG_GREYBUS_GPIO,                                                    \
			   (DT_FOREACH_PROP_ELEM_SEP_VARGS(_node_id, gpio_controllers, _GB_CPORT,  \
							   (, ), _bundle, GREYBUS_PROTOCOL_GPIO,   \
							   &gb_gpio_driver,                        \
							   GB_CPORT_GPIO_PRIV_DATA))),             \
		IF_ENABLED(CONFIG_GREYBUS_PWM, (DT_FOREACH_PROP_ELEM_SEP_VARGS(                    \
						       _node_id, pwm_controllers, _GB_CPORT, (, ), \
						       _bundle, GREYBUS_PROTOCOL_PWM,              \
						       &gb_pwm_driver, GB_CPORT_PWM_PRIV_DATA))),  \
		IF_ENABLED(CONFIG_GREYBUS_SPI, (DT_FOREACH_PROP_ELEM_SEP_VARGS(                    \
						       _node_id, spi_controllers, _GB_CPORT, (, ), \
						       _bundle, GREYBUS_PROTOCOL_SPI,              \
						       &gb_spi_driver, GB_CPORT_SPI_PRIV_DATA))),  \
		IF_ENABLED(CONFIG_GREYBUS_UART,                                                    \
			   (DT_FOREACH_PROP_ELEM_SEP_VARGS(_node_id, uart_controllers, _GB_CPORT,  \
							   (, ), _bundle, GREYBUS_PROTOCOL_UART,   \
							   &gb_uart_driver,                        \
							   GB_CPORT_DEV_PRIV_DATA))),              \
		IF_ENABLED(CONFIG_GREYBUS_I2C, (DT_FOREACH_PROP_ELEM_SEP_VARGS(                    \
						       _node_id, i2c_controllers, _GB_CPORT, (, ), \
						       _bundle, GREYBUS_PROTOCOL_I2C,              \
						       &gb_i2c_driver, GB_CPORT_DEV_PRIV_DATA))))

#define GREYBUS_CPORT_IN_LIGHTS(_node_id, _bundle)                                                 \
	IF_ENABLED(CONFIG_GREYBUS_LIGHTS, (GB_CPORT(&gb_lights_priv_data, _bundle,                 \
						    GREYBUS_PROTOCOL_LIGHTS, &gb_lights_driver)))

#define GREYBUS_CPORT_IN_VIBRATORS(_node_id, _bundle)                                              \
	IF_ENABLED(CONFIG_GREYBUS_VIBRATOR,                                                        \
		   (DT_FOREACH_PROP_ELEM_SEP_VARGS(_node_id, vibrators, _GB_CPORT, (, ), _bundle,  \
						   GREYBUS_PROTOCOL_VIBRATOR, &gb_vibrator_driver, \
						   GB_CPORT_DEV_PRIV_DATA)))

#define GB_CPORTS_IN_BUNDLE(node_id, bundle)                                                       \
	COND_CODE_1(                                                                               \
		DT_NODE_HAS_COMPAT_STATUS(node_id, zephyr_greybus_bundle_bridged_phy, okay),       \
		(GREYBUS_CPORTS_IN_BRIDGED_PHY_BUNDLE(node_id, bundle)),                           \
		(COND_CODE_1(                                                                      \
			DT_NODE_HAS_COMPAT_STATUS(node_id, zephyr_greybus_bundle_lights, okay),    \
			(GREYBUS_CPORT_IN_LIGHTS(node_id, bundle)),                                \
			(IF_ENABLED(DT_NODE_HAS_COMPAT_STATUS(                                     \
					    node_id, zephyr_greybus_bundle_vibrator, okay),        \
				    (GREYBUS_CPORT_IN_VIBRATORS(node_id, bundle)))))))

/* Requred for counter based naming to work */
#define GB_CPORTS_BUNDLE_WRAPPER(node_id) GB_CPORTS_IN_BUNDLE(node_id, LOCAL_COUNTER)

#define GB_CPORTS_FW(_bundle)                                                                      \
	GB_CPORT(NULL, _bundle, GREYBUS_PROTOCOL_FIRMWARE_MANAGEMENT, &gb_fw_mgmt_driver),         \
		GB_CPORT(NULL, _bundle, GREYBUS_PROTOCOL_FIRMWARE_DOWNLOAD,                        \
			 &gb_fw_download_driver)

#ifdef CONFIG_GREYBUS_RAW
static struct gb_raw_driver_data gb_raw_driver_data_array[GREYBUS_RAW_CPORT_COUNT];
#endif // CONFIG_GREYBUS_RAW

#define GB_CPORT_RAW_HANDLER(_i, _bundle)                                                          \
	GB_CPORT(&gb_raw_driver_data_array[_i], _bundle, GREYBUS_PROTOCOL_RAW, &gb_raw_driver)

#define GB_CPORTS_RAW(_bundle) LISTIFY(GREYBUS_RAW_CPORT_COUNT, GB_CPORT_RAW_HANDLER, (, ), _bundle)

static struct gb_cport cports[] = {
	/* cport0 is always control cport */
	GB_CPORT(NULL, LOCAL_COUNTER, GREYBUS_PROTOCOL_CONTROL, &gb_control_driver),
#ifdef CONFIG_GREYBUS_FW
	GB_CPORTS_FW(LOCAL_COUNTER),
#endif // CONFIG_GREYBUS_FW
#ifdef CONFIG_GREYBUS_LOG_BACKEND
	GB_CPORT(NULL, LOCAL_COUNTER, GREYBUS_PROTOCOL_LOG, &gb_log_driver),
#endif // CONFIG_GREYBUS_LOG
#ifdef CONFIG_GREYBUS_RAW
	GB_CPORTS_RAW(LOCAL_COUNTER),
#endif // CONFIG_GREYBUS_RAW
#ifdef CONFIG_GREYBUS_LOOPBACK
	GB_CPORT(NULL, LOCAL_COUNTER, GREYBUS_PROTOCOL_LOOPBACK, &gb_loopback_driver),
#endif // CONFIG_GREYBUS_LOOPBACK
	DT_FOREACH_CHILD_STATUS_OKAY(_GREYBUS_BASE_NODE, GB_CPORTS_BUNDLE_WRAPPER)};

BUILD_ASSERT(GREYBUS_CPORT_COUNT == ARRAY_SIZE(cports));

const struct gb_cport *gb_cport_get(uint16_t cport)
{
	return (cport >= GREYBUS_CPORT_COUNT) ? NULL : &cports[cport];
}

int gb_cports_init()
{
	return 0;
}

void gb_cports_deinit()
{
}
