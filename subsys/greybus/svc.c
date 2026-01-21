#include "greybus/greybus_messages.h"
#include "greybus/greybus_protocols.h"
#include <zephyr/kernel.h>
#include <greybus/svc.h>
#include <greybus/apbridge.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(greybus_svc, CONFIG_GREYBUS_LOG_LEVEL);

#define ENDO_ID              0x4755
#define GB_SVC_VERSION_MAJOR 0x00
#define GB_SVC_VERSION_MINOR 0x01

#define SVC_INIT_TIMEOUT K_SECONDS(5)

K_SEM_DEFINE(svc_init, 0, 1);

/* TODO: Add support for standalone SVC support */
static int gb_svc_msg_send(struct gb_message *msg)
{
	return gb_apbridge_send(SVC_INF_ID, 0, msg);
}

static void svc_response_helper(struct gb_message *msg, const void *payload, size_t payload_len,
				uint8_t status)
{
	int ret;
	struct gb_message *resp = gb_message_response_alloc(payload, payload_len, msg->header.type,
							    msg->header.operation_id, status);
	if (resp == NULL) {
		LOG_ERR("Failed to allocate response for %X", msg->header.type);
		return;
	}
	ret = gb_svc_msg_send(resp);
	if (ret < 0) {
		LOG_ERR("Failed to send SVC message");
	}
}

static void svc_intf_set_pwrm_handler(struct gb_message *msg)
{
	uint8_t tx_mode, rx_mode;
	struct gb_svc_intf_set_pwrm_response resp = {.result_code = GB_SVC_SETPWRM_PWR_LOCAL};
	struct gb_svc_intf_set_pwrm_request *req =
		(struct gb_svc_intf_set_pwrm_request *)msg->payload;
	tx_mode = req->tx_mode;
	rx_mode = req->rx_mode;

	if (tx_mode == GB_SVC_UNIPRO_HIBERNATE_MODE && rx_mode == GB_SVC_UNIPRO_HIBERNATE_MODE) {
		resp.result_code = GB_SVC_SETPWRM_PWR_OK;
	}

	svc_response_helper(msg, &resp, sizeof(struct gb_svc_intf_set_pwrm_response),
			    GB_SVC_OP_SUCCESS);
}

static void svc_intf_vsys_enable_disable_handler(struct gb_message *msg)
{
	struct gb_svc_intf_vsys_response resp = {.result_code = GB_SVC_INTF_VSYS_OK};

	svc_response_helper(msg, &resp, sizeof(struct gb_svc_intf_vsys_response),
			    GB_SVC_OP_SUCCESS);
}

static void svc_interface_refclk_enable_disable_handler(struct gb_message *msg)
{
	struct gb_svc_intf_refclk_response resp = {.result_code = GB_SVC_INTF_REFCLK_OK};

	svc_response_helper(msg, &resp, sizeof(struct gb_svc_intf_refclk_response),
			    GB_SVC_OP_SUCCESS);
}

static void svc_interface_unipro_enable_disable_handler(struct gb_message *msg)
{
	struct gb_svc_intf_unipro_response resp = {.result_code = GB_SVC_INTF_UNIPRO_OK};

	svc_response_helper(msg, &resp, sizeof(struct gb_svc_intf_unipro_response),
			    GB_SVC_OP_SUCCESS);
}

static void svc_connection_create_handler(struct gb_message *msg)
{
	int ret;
	struct gb_svc_conn_create_request *req = (struct gb_svc_conn_create_request *)msg->payload;

	if (req->intf1_id == req->intf2_id && req->cport1_id == req->cport2_id) {
		LOG_ERR("Cannot create loop connection");
		goto fail;
	}

	ret = gb_apbridge_connection_create(req->intf1_id, req->cport1_id, req->intf2_id,
					    req->cport2_id);
	if (ret < 0) {
		LOG_ERR("Failed to create connection");
		goto fail;
	}

	LOG_DBG("Created connection between Intf %u, Cport %u and Intf %u, Cport %u", req->intf1_id,
		req->cport1_id, req->intf2_id, req->cport2_id);

	svc_response_helper(msg, NULL, 0, GB_SVC_OP_SUCCESS);
	return;

fail:
	svc_response_helper(msg, NULL, 0, GB_SVC_OP_UNKNOWN_ERROR);
}

static void svc_connection_destroy_handler(struct gb_message *msg)
{
	int ret;
	struct gb_svc_conn_destroy_request *req =
		(struct gb_svc_conn_destroy_request *)msg->payload;

	LOG_DBG("Destroy connection between Intf %u, Cport %u and Intf %u, Cport %u", req->intf1_id,
		req->cport1_id, req->intf2_id, req->cport2_id);
	ret = gb_apbridge_connection_destroy(req->intf1_id, req->cport1_id, req->intf2_id,
					     req->cport2_id);
	if (ret < 0) {
		LOG_ERR("Failed to destroy connection %d between Cport 1: %u of Interface 1: %u "
			"and Cport 2: %u of Interface 2: %u",
			ret, req->cport1_id, req->intf1_id, req->cport2_id, req->intf2_id);
		goto fail;
	}

	svc_response_helper(msg, NULL, 0, GB_SVC_OP_SUCCESS);
	return;

fail:
	svc_response_helper(msg, NULL, 0, GB_SVC_OP_UNKNOWN_ERROR);
}

static void svc_dme_peer_get_handler(struct gb_message *msg)
{
	struct gb_svc_dme_peer_get_response resp = {.result_code = 0, .attr_value = 0x0126};

	svc_response_helper(msg, &resp, sizeof(struct gb_svc_dme_peer_get_response),
			    GB_SVC_OP_SUCCESS);
}

static void svc_dme_peer_set_handler(struct gb_message *msg)
{
	struct gb_svc_dme_peer_set_response resp = {.result_code = 0};

	svc_response_helper(msg, &resp, sizeof(struct gb_svc_dme_peer_set_response),
			    GB_SVC_OP_SUCCESS);
}

static void svc_pwrm_get_rail_count_handler(struct gb_message *msg)
{
	struct gb_svc_pwrmon_rail_count_get_response req = {.rail_count = 0};

	svc_response_helper(msg, &req, sizeof(struct gb_svc_pwrmon_rail_count_get_response),
			    GB_SVC_OP_SUCCESS);
}

/* TODO: Some interfaces will probably want to use this */
static void svc_interface_activate_handler(struct gb_message *msg)
{
	struct gb_svc_intf_activate_response resp = {.status = GB_SVC_OP_SUCCESS,
						     .intf_type = GB_SVC_INTF_TYPE_GREYBUS};
	svc_response_helper(msg, &resp, sizeof(struct gb_svc_intf_activate_response),
			    GB_SVC_OP_SUCCESS);
}

static void svc_interface_resume_handler(struct gb_message *msg)
{
	struct gb_svc_intf_resume_response resp = {.status = GB_SVC_OP_SUCCESS};

	svc_response_helper(msg, &resp, sizeof(struct gb_svc_intf_resume_response),
			    GB_SVC_OP_SUCCESS);
}

static int svc_send_hello(void)
{
	const struct gb_svc_hello_request req_data = {.endo_id = ENDO_ID,
						      .interface_id = AP_INF_ID};
	struct gb_message *req = gb_message_request_alloc_with_payload(
		&req_data, sizeof(req_data), GB_SVC_TYPE_SVC_HELLO, false);

	if (!req) {
		return -ENOMEM;
	}

	return gb_svc_msg_send(req);
}

static void svc_version_response_handler(struct gb_message *msg)
{
	svc_send_hello();
}

static void svc_module_inserted_response_handler(struct gb_message *msg)
{
	if (!gb_message_is_success(msg)) {
		/* TODO: Add functionality to remove the interface in case of error */
		LOG_ERR("Module Inserted Event failed");
	}
}

static void svc_module_removed_response_handler(struct gb_message *msg)
{
	if (!gb_message_is_success(msg)) {
		LOG_DBG("Module Removal Failed");
	}
}

static void gb_handle_msg(struct gb_message *msg)
{
	switch (gb_message_type(msg)) {
	case GB_SVC_TYPE_INTF_DEVICE_ID:
	case GB_SVC_TYPE_ROUTE_CREATE:
	case GB_SVC_TYPE_ROUTE_DESTROY:
	case GB_SVC_TYPE_PING:
		svc_response_helper(msg, NULL, 0, GB_OP_SUCCESS);
		break;
	case GB_SVC_TYPE_CONN_CREATE:
		svc_connection_create_handler(msg);
		break;
	case GB_SVC_TYPE_CONN_DESTROY:
		svc_connection_destroy_handler(msg);
		break;
	case GB_SVC_TYPE_DME_PEER_GET:
		svc_dme_peer_get_handler(msg);
		break;
	case GB_SVC_TYPE_DME_PEER_SET:
		svc_dme_peer_set_handler(msg);
		break;
	case GB_SVC_TYPE_INTF_SET_PWRM:
		svc_intf_set_pwrm_handler(msg);
		break;
	case GB_SVC_TYPE_PWRMON_RAIL_COUNT_GET:
		svc_pwrm_get_rail_count_handler(msg);
		break;
	case GB_SVC_TYPE_INTF_VSYS_ENABLE:
	case GB_SVC_TYPE_INTF_VSYS_DISABLE:
		svc_intf_vsys_enable_disable_handler(msg);
		break;
	case GB_SVC_TYPE_INTF_REFCLK_ENABLE:
	case GB_SVC_TYPE_INTF_REFCLK_DISABLE:
		svc_interface_refclk_enable_disable_handler(msg);
		break;
	case GB_SVC_TYPE_INTF_UNIPRO_ENABLE:
	case GB_SVC_TYPE_INTF_UNIPRO_DISABLE:
		svc_interface_unipro_enable_disable_handler(msg);
		break;
	case GB_SVC_TYPE_INTF_ACTIVATE:
		svc_interface_activate_handler(msg);
		break;
	case GB_SVC_TYPE_INTF_RESUME:
		svc_interface_resume_handler(msg);
		break;
	case GB_RESPONSE(GB_SVC_TYPE_PROTOCOL_VERSION):
		svc_version_response_handler(msg);
		break;
	case GB_RESPONSE(GB_SVC_TYPE_SVC_HELLO):
		k_sem_give(&svc_init);
		break;
	case GB_RESPONSE(GB_SVC_TYPE_MODULE_INSERTED):
		svc_module_inserted_response_handler(msg);
		break;
	case GB_RESPONSE(GB_SVC_TYPE_MODULE_REMOVED):
		svc_module_removed_response_handler(msg);
		break;
	default:
		LOG_WRN("Handling SVC operation Type %X not supported yet", msg->header.type);
	}
}

static int gb_svc_intf_write(struct gb_interface *intf, struct gb_message *msg, uint16_t cport)
{
	ARG_UNUSED(intf);
	ARG_UNUSED(cport);

	gb_handle_msg(msg);
	gb_message_dealloc(msg);
	return 0;
}

static struct gb_interface svc_intf = {
	.id = SVC_INF_ID,
	.write = gb_svc_intf_write,
	.create_connection = NULL,
	.destroy_connection = NULL,
	.ctrl_data = NULL,
};

static int gb_svc_send_version(void)
{
	struct gb_message *req;
	struct gb_svc_version_request req_data = {.major = GB_SVC_VERSION_MAJOR,
						  .minor = GB_SVC_VERSION_MINOR};

	req = gb_message_request_alloc_with_payload(&req_data, sizeof(req_data),
						    GB_SVC_TYPE_PROTOCOL_VERSION, false);
	if (!req) {
		return -ENOMEM;
	}

	return gb_svc_msg_send(req);
}

int gb_svc_init(void)
{
	int ret;

	gb_interface_add(&svc_intf);

	ret = gb_apbridge_connection_create(AP_INF_ID, 0, SVC_INF_ID, 0);
	if (ret < 0) {
		LOG_ERR("Failed to create AP SVC connection");
		return ret;
	}

	ret = gb_svc_send_version();
	if (ret < 0) {
		LOG_ERR("Failed to send SVC version request");
		return ret;
	}

	ret = k_sem_take(&svc_init, SVC_INIT_TIMEOUT);
	if (ret < 0) {
		LOG_ERR("Failed to init");
		return ret;
	}

	return 0;
}

void gb_svc_deinit(void)
{
	gb_interface_remove(svc_intf.id);
}

int gb_svc_send_module_inserted(uint8_t primary_intf_id, uint8_t intf_count, uint16_t flags)
{
	struct gb_message *req;
	const struct gb_svc_module_inserted_request req_data = {
		.primary_intf_id = primary_intf_id,
		.intf_count = intf_count,
		.flags = flags,
	};

	req = gb_message_request_alloc_with_payload(&req_data, sizeof(req_data),
						    GB_SVC_TYPE_MODULE_INSERTED, false);
	if (!req) {
		return -ENOMEM;
	}

	return gb_svc_msg_send(req);
}

int gb_svc_send_module_removed(uint8_t primary_intf_id)
{
	struct gb_message *req;
	const struct gb_svc_module_removed_request req_data = {.primary_intf_id = primary_intf_id};

	req = gb_message_request_alloc_with_payload(&req_data, sizeof(req_data),
						    GB_SVC_TYPE_MODULE_REMOVED, false);
	if (!req) {
		return -ENOMEM;
	}

	return gb_svc_msg_send(req);
}
