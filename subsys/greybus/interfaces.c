#include <greybus/apbridge.h>
#include <zephyr/sys/errno_private.h>
#include "greybus_heap.h"
#include <zephyr/sys/util.h>
#include <zephyr/sys/mutex.h>

static struct gb_interface *intfs[AP_MAX_NODES];
K_MUTEX_DEFINE(intfs_mutex);

static int new_interface_id(void)
{
	int ret, i;

	ret = k_mutex_lock(&intfs_mutex, K_NO_WAIT);
	if (ret < 0) {
		return -EBUSY;
	}

	ret = -EOVERFLOW;

	for (i = INTF_START_ID; i < ARRAY_SIZE(intfs); i++) {
		if (!intfs[i]) {
			ret = i;
			goto unlock;
		}
	}

unlock:
	k_mutex_unlock(&intfs_mutex);
	return ret;
}

int gb_interface_add(struct gb_interface *intf)
{
	int ret;

	ret = k_mutex_lock(&intfs_mutex, K_NO_WAIT);
	if (ret < 0) {
		return -EBUSY;
	}

	if (intfs[intf->id]) {
		ret = -EALREADY;
	} else {
		ret = 0;
		intfs[intf->id] = intf;
	}

	k_mutex_unlock(&intfs_mutex);

	return ret;
}

void gb_interface_remove(uint8_t id)
{
	k_mutex_lock(&intfs_mutex, K_FOREVER);

	intfs[id] = NULL;

	k_mutex_unlock(&intfs_mutex);
}

struct gb_interface *gb_interface_alloc(gb_controller_write_callback_t write_cb,
					gb_controller_create_connection_t create_connection_cb,
					gb_controller_destroy_connection_t destroy_connection_cb,
					void *ctrl_data)
{
	int ret;
	struct gb_interface *intf;

	ret = k_mutex_lock(&intfs_mutex, K_NO_WAIT);
	if (ret < 0) {
		return NULL;
	}

	ret = new_interface_id();
	if (ret < 0) {
		k_mutex_unlock(&intfs_mutex);
		return NULL;
	}

	intf = gb_alloc(sizeof(struct gb_interface));
	if (!intf) {
		k_mutex_unlock(&intfs_mutex);
		return intf;
	}

	intf->id = ret;
	intf->create_connection = create_connection_cb;
	intf->destroy_connection = destroy_connection_cb;
	intf->write = write_cb;
	intf->ctrl_data = ctrl_data;

	gb_interface_add(intf);

	k_mutex_unlock(&intfs_mutex);

	return intf;
}

void gb_interface_dealloc(struct gb_interface *intf)
{
	gb_interface_remove(intf->id);
	gb_free(intf);
}

struct gb_interface *gb_interface_get(uint8_t id)
{
	return intfs[id];
}
