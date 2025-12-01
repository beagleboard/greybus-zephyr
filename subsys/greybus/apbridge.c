#include <greybus/apbridge.h>
#include <zephyr/sys/errno_private.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(greybus_apbridge, CONFIG_GREYBUS_LOG_LEVEL);

struct node_ap_item {
	struct gb_interface *node_intf;
	uint16_t node_cport;
};

static struct node_ap_item node_ap_map[AP_MAX_NODES] = {0};

static int node_ap_add(uint16_t ap_cport, uint16_t node_cport, struct gb_interface *node_intf)
{
	if (ap_cport >= ARRAY_SIZE(node_ap_map)) {
		return -EOVERFLOW;
	}

	if (node_ap_map[ap_cport].node_intf) {
		return -EALREADY;
	}

	node_ap_map[ap_cport].node_cport = node_cport;
	node_ap_map[ap_cport].node_intf = node_intf;

	return 0;
}

static int node_ap_remove(uint16_t ap_cport)
{
	if (ap_cport >= ARRAY_SIZE(node_ap_map)) {
		return -EOVERFLOW;
	}

	node_ap_map[ap_cport].node_cport = 0;
	node_ap_map[ap_cport].node_intf = NULL;

	return 0;
}

static int node_to_ap_cport(uint8_t node_id, uint16_t node_cport)
{
	const struct node_ap_item *item;

	for (size_t i = 0; i < ARRAY_SIZE(node_ap_map); i++) {
		item = &node_ap_map[i];

		if (item->node_intf && item->node_intf->id == node_id &&
		    item->node_cport == node_cport) {
			return i;
		}
	}

	return -EINVAL;
}

int gb_apbridge_init(void)
{
	return 0;
}

void gb_apbridge_deinit(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(node_ap_map); ++i) {
		node_ap_map[i].node_cport = 0;
		node_ap_map[i].node_intf = NULL;
	}
}

int gb_apbridge_connection_create(uint8_t intf1_id, uint16_t intf1_cport, uint8_t intf2_id,
				  uint16_t intf2_cport)
{
	struct gb_interface *intf;
	uint8_t node_id;
	uint16_t ap_cport, node_cport;
	int ret;

	if (intf1_id == AP_INF_ID) {
		node_id = intf2_id;
		node_cport = intf2_cport;
		ap_cport = intf1_cport;
	} else if (intf2_id == AP_INF_ID) {
		node_id = intf1_id;
		node_cport = intf1_cport;
		ap_cport = intf2_cport;
	} else {
		LOG_ERR("Cannot create connection between two non-AP");
		return -EINVAL;
	}

	intf = gb_interface_get(node_id);
	if (!intf) {
		LOG_ERR("Failed to find node interface");
		return -EINVAL;
	}

	/* create_connection is optional */
	if (intf->create_connection) {
		ret = intf->create_connection(intf, node_cport);
		if (ret < 0) {
			LOG_ERR("Failed to create node connection");
			return ret;
		}
	}

	ret = node_ap_add(ap_cport, node_cport, intf);
	if (ret < 0) {
		LOG_ERR("Failed to add AP to node");
		return ret;
	}

	return 0;
}

int gb_apbridge_connection_destroy(uint8_t intf1_id, uint16_t intf1_cport, uint8_t intf2_id,
				   uint16_t intf2_cport)
{
	uint8_t node_id;
	uint16_t ap_cport, node_cport;
	const struct gb_interface *intf;

	if (intf1_id == AP_INF_ID) {
		node_id = intf2_id;
		node_cport = intf2_cport;
		ap_cport = intf1_cport;
	} else if (intf2_id == AP_INF_ID) {
		node_id = intf1_id;
		node_cport = intf1_cport;
		ap_cport = intf2_cport;
	} else {
		LOG_ERR("Cannot destroy connection between two non-AP");
		return -EINVAL;
	}

	intf = gb_interface_get(node_id);
	/* Ignore if intf has already been cleaned up, or if destroy_connection is not defined */
	if (intf && intf->destroy_connection) {
		intf->destroy_connection(intf, node_cport);
	}

	node_ap_remove(ap_cport);

	return 0;
}

int gb_apbridge_send(uint8_t intf_id, uint16_t intf_cport, struct gb_message *msg)
{
	struct gb_interface *intf;
	uint16_t target_cport;
	int ret;

	if (intf_id == AP_INF_ID) {
		intf = node_ap_map[intf_cport].node_intf;
		target_cport = node_ap_map[intf_cport].node_cport;
	} else {
		ret = node_to_ap_cport(intf_id, intf_cport);
		if (ret < 0) {
			LOG_ERR("Failed to find AP cport");
			return ret;
		}

		intf = gb_interface_get(AP_INF_ID);
		target_cport = ret;
	}

	return intf->write(intf, msg, target_cport);
}
