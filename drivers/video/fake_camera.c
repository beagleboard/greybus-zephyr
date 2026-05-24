#define DT_DRV_COMPAT zephyr_fake_camera

#include <zephyr/device.h>
#include <zephyr/drivers/video.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <string.h>

LOG_MODULE_REGISTER(fake_cam, LOG_LEVEL_INF);

struct cam_data {
	struct video_format fmt;
	struct k_fifo in_queue;
	struct k_fifo out_queue;
	struct k_timer frame_timer;
};

static void frame_timer_handler(struct k_timer *timer)
{
	struct cam_data *data = CONTAINER_OF(timer, struct cam_data, frame_timer);
	struct video_buffer *vbuf;

	vbuf = k_fifo_get(&data->in_queue, K_NO_WAIT);
	if (vbuf == NULL) {
		LOG_WRN("No buffer available in queue");
		return;
	}

	if (vbuf->type != VIDEO_BUF_TYPE_OUTPUT) {
		LOG_WRN("Unexpected buffer type: %d", vbuf->type);
		k_fifo_put(&data->out_queue, vbuf);
		return;
	}

	memset(vbuf->buffer, 0xAA, vbuf->size);
	vbuf->bytesused = vbuf->size;
	vbuf->timestamp = k_uptime_get_32();

	k_fifo_put(&data->out_queue, vbuf);
	LOG_INF("Generated dummy frame (%d bytes)", vbuf->bytesused);
}

static int cam_enqueue(const struct device *dev, struct video_buffer *vbuf)
{
	struct cam_data *data = dev->data;

	if (vbuf == NULL || vbuf->buffer == NULL || vbuf->size == 0) {
		return -EINVAL;
	}

	k_fifo_put(&data->in_queue, vbuf);
	return 0;
}

static int cam_dequeue(const struct device *dev, struct video_buffer **vbuf, k_timeout_t timeout)
{
	struct cam_data *data = dev->data;

	*vbuf = k_fifo_get(&data->out_queue, timeout);
	if (*vbuf == NULL) {
		return -EAGAIN;
	}
	return 0;
}

static int cam_set_stream(const struct device *dev, bool enable, enum video_buf_type type)
{
	struct cam_data *data = dev->data;

	if (type != VIDEO_BUF_TYPE_OUTPUT) {
		return -EINVAL;
	}

	if (enable) {
		LOG_INF("Stream started");
		k_timer_start(&data->frame_timer, K_MSEC(100), K_MSEC(100));
	} else {
		LOG_INF("Stream stopped");
		k_timer_stop(&data->frame_timer);
	}
	return 0;
}

static int cam_get_caps(const struct device *dev, struct video_caps *caps)
{
	if (caps->type != VIDEO_BUF_TYPE_OUTPUT) {
		return -EINVAL;
	}
	caps->min_vbuf_count = 1;
	return 0;
}

static int cam_set_format(const struct device *dev, struct video_format *fmt)
{
	struct cam_data *data = dev->data;
	if (fmt->type != VIDEO_BUF_TYPE_OUTPUT) {
		return -EINVAL;
	}
	data->fmt = *fmt;
	return 0;
}

static int cam_get_format(const struct device *dev, struct video_format *fmt)
{
	struct cam_data *data = dev->data;
	if (fmt->type != VIDEO_BUF_TYPE_OUTPUT) {
		return -EINVAL;
	}
	*fmt = data->fmt;
	return 0;
}

static DEVICE_API(video, cam_driver_api) = {
	.set_format = cam_set_format,
	.get_format = cam_get_format,
	.get_caps = cam_get_caps,
	.set_stream = cam_set_stream,
	.enqueue = cam_enqueue,
	.dequeue = cam_dequeue,
};

static int cam_init(const struct device *dev)
{
	struct cam_data *data = dev->data;

	k_fifo_init(&data->in_queue);
	k_fifo_init(&data->out_queue);
	k_timer_init(&data->frame_timer, frame_timer_handler, NULL);

	LOG_INF("init: camera driver initialised.");
	return 0;
}

static struct cam_data cam_data_0;

DEVICE_DT_INST_DEFINE(0, cam_init, NULL, &cam_data_0, NULL, POST_KERNEL, CONFIG_VIDEO_INIT_PRIORITY,
		      &cam_driver_api);
