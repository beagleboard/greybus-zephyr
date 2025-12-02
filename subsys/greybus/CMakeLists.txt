# SPDX-License-Identifier: BSD-3-Clause

zephyr_include_directories(../../../include)

zephyr_library()

zephyr_library_sources(
  greybus_messages.c
  greybus_heap.c
  platform/certificate.c
)

# Node-specific files
zephyr_library_sources_ifdef(
	CONFIG_GREYBUS_NODE
	greybus_transport.c
	greybus-core.c
	control-gpb.c
	platform/manifest.c
	platform/service.c
	greybus_cport.c
)

# APBridge-specific files
zephyr_library_sources_ifdef(
	CONFIG_GREYBUS_APBRIDGE
	apbridge.c
	interfaces.c
)

# SVC-specific files
zephyr_library_sources_ifdef(
	CONFIG_GREYBUS_SVC
	svc.c
)

if(CONFIG_GREYBUS_TLS_BUILTIN)
  set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated/)

  if(CONFIG_GREYBUS_TLS_CLIENT_VERIFY_OPTIONAL
     OR CONFIG_GREYBUS_TLS_CLIENT_VERIFY_REQUIRED)
    generate_inc_file_for_target(app ${CONFIG_GREYBUS_TLS_BUILTIN_CA_CERT}
                                 ${gen_dir}/greybus_tls_builtin_ca_cert.inc)
  endif()

  generate_inc_file_for_target(app ${CONFIG_GREYBUS_TLS_BUILTIN_SERVER_CERT}
                               ${gen_dir}/greybus_tls_builtin_server_cert.inc)

  generate_inc_file_for_target(
    app ${CONFIG_GREYBUS_TLS_BUILTIN_SERVER_PRIVKEY}
    ${gen_dir}/greybus_tls_builtin_server_privkey.inc)
endif()

# Transports
zephyr_library_sources_ifdef(CONFIG_GREYBUS_XPORT_TCPIP transport/tcpip.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_XPORT_DUMMY transport/dummy.c)

# Protocols
zephyr_library_sources_ifdef(CONFIG_GREYBUS_AUDIO audio.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_CAMERA camera.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_GPIO gpio.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_HID hid.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_I2C i2c.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_LIGHTS lights.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_LOOPBACK loopback.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_POWER_SUPPLY power_supply.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_PWM pwm-protocol.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_SDIO sdio.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_SPI spi.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_UART uart.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_USB usb.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_VIBRATOR vibrator.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_FW fw_management.c fw_download.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_LOG_BACKEND log.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_RAW raw.c)
