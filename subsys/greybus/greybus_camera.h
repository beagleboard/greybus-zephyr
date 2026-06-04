#ifndef __GREYBUS_CAMERA_H__
#define __GREYBUS_CAMERA_H__

#include <zephyr/toolchain.h>
#include <stdint.h>

#define GB_CAMERA_TYPE_PROTOCOL_VERSION     0x01
#define GB_CAMERA_TYPE_CAPABILITIES         0x02
#define GB_CAMERA_TYPE_CONFIGURE_STREAMS    0x03
#define GB_CAMERA_TYPE_CAPTURE              0x04
#define GB_CAMERA_TYPE_FLUSH                0x05

/* 0x01: Protocol Version Response */
struct gb_camera_version_response {
    uint8_t major;
    uint8_t minor;
} __packed;

/* 0x02: Capabilities Response */
struct gb_camera_capabilities_response {
    uint8_t capabilities[0]; 
} __packed;

#endif /* __GREYBUS_CAMERA_H__ */
