#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/video.h>

#define VBUF_SIZE 1024
static uint8_t raw_buffer[VBUF_SIZE];
static struct video_buffer vbuf;

int main(void)
{
	const struct device *dev = DEVICE_DT_GET(DT_ALIAS(camera0));

	if (!device_is_ready(dev)) {
		printk("Error: Camera device not ready\n");
		return 0;
	}
	printk("Success: Found Fake Camera Device!\n");

	vbuf.buffer = raw_buffer;
	vbuf.size = VBUF_SIZE;
	vbuf.bytesused = 0;

	video_enqueue(dev, &vbuf);
	printk("App: Queued empty buffer.\n");

	//'0' as the type parameter
	video_stream_start(dev, 0);

	struct video_buffer *rx_buf;
	printk("Waiting for frame\n");

	int err = video_dequeue(dev, &rx_buf, K_MSEC(500));

	if (err == 0) {
		printk("SUCCESS! Received Frame!\n");
		printk("Frame Size: %d bytes\n", rx_buf->bytesused);
		printk("Data Byte 0: 0x%02X\n", rx_buf->buffer[0]);
	} else {
		printk("Error: Failed to dequeue frame (%d)\n", err);
	}
	video_stream_stop(dev, 0);
	return 0;
}
