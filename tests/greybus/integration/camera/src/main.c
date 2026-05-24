#include <zephyr/ztest.h>
#include <zephyr/drivers/video.h>
#include <zephyr/device.h>

#define VBUF_SIZE 1024

static struct video_buffer *vbuf;

ZTEST(camera_suite, test_camera_device_ready)
{
	const struct device *dev = DEVICE_DT_GET(DT_ALIAS(camera0));
	zassert_true(device_is_ready(dev), "Camera device not ready");
}

ZTEST(camera_suite, test_camera_frame_capture)
{
	const struct device *dev = DEVICE_DT_GET(DT_ALIAS(camera0));
	struct video_buffer *rx_buf;
	int err;

	vbuf = video_buffer_alloc(VBUF_SIZE, K_NO_WAIT);
	zassert_not_null(vbuf, "Failed to allocate video buffer");

	vbuf->type = VIDEO_BUF_TYPE_OUTPUT;

	video_enqueue(dev, vbuf);
	video_stream_start(dev, VIDEO_BUF_TYPE_OUTPUT);

	err = video_dequeue(dev, &rx_buf, K_MSEC(500));
	zassert_equal(err, 0, "Failed to dequeue (err: %d)", err);
	zassert_equal(rx_buf->bytesused, VBUF_SIZE, "Wrong frame size");

	video_stream_stop(dev, VIDEO_BUF_TYPE_OUTPUT);
	video_buffer_release(vbuf);
}

ZTEST_SUITE(camera_suite, NULL, NULL, NULL, NULL, NULL);
