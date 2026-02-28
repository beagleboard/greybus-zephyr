#include "greybus/greybus_messages.h"
#include "greybus/greybus_protocols.h"
#include <zephyr/ztest.h>
#include <greybus/greybus.h>

struct gb_msg_with_cport gb_transport_get_message(void);

ZTEST_SUITE(greybus_lights_tests, NULL, NULL, NULL, NULL, NULL);

// TODO Implement test for get_channel_config

ZTEST(greybus_lights_tests, test_get_lights){
    struct gb_msg_with_cport resp;
    struct gb_message *req;

    req = gb_message_request_alloc(0, GB_LIGHTS_TYPE_GET_LIGHTS, false);

    greybus_rx_handler(1, req);

    resp = gb_transport_get_message();

    zassert_true(gb_message_is_success(resp.msg), "Operation Get Lights Failed");

    struct gb_lights_get_lights_response *payload = (void *)resp.msg->payload;

    zassert_equal(payload->lights_count, 1, "Expected 1 light, but got %d", payload->lights_count);

    gb_message_dealloc(resp.msg);
}

ZTEST(greybus_lights_tests, test_get_light_config){
    struct gb_msg_with_cport resp;
    struct gb_message *req;

    req = gb_message_request_alloc(sizeof(struct gb_lights_get_light_config_request), GB_LIGHTS_TYPE_GET_LIGHT_CONFIG, false);

    struct gb_lights_get_light_config_request *req_payload = (void *)req->payload;
    req_payload->id = 0;

    greybus_rx_handler(1, req);

    resp = gb_transport_get_message();

    struct gb_lights_get_light_config_response *resp_payload = (void *)resp.msg->payload;
    zassert_true(gb_message_is_success(resp.msg), "Operation Lights Get Config Failed");

    zassert_equal(resp_payload->channel_count, 1, "Expeced 1 channel, got %d", resp_payload->channel_count);
    zassert_ok(strcmp(resp_payload->name, "leds"), "Expected name: leds, got %s", resp_payload->name);

    gb_message_dealloc(resp.msg);

}

ZTEST(greybus_lights_tests, test_set_brightness){

    struct gb_msg_with_cport resp;
    struct gb_message *req;

    req = gb_message_request_alloc(
        sizeof(struct gb_lights_set_brightness_request),
            GB_LIGHTS_TYPE_SET_BRIGHTNESS,
            false);

    struct gb_lights_set_brightness_request *payload = (void *)req->payload;
	payload->light_id = 0;
	payload->brightness = 50;

	greybus_rx_handler(1, req);

	resp = gb_transport_get_message();

	zassert_true(gb_message_is_success(resp.msg), "Operation Lights Set Brightness failed");

	gb_message_dealloc(resp.msg);
}
