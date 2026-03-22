/*
 * Copyright (c) 2025 Ayush Singh, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <greybus/greybus.h>
#include <zephyr/kernel.h>
#include <zephyr/net/dns_sd.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include "../platform/certificate.h"
#include <greybus/greybus_messages.h>
#include "../greybus_internal.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(greybus_transport_tcpip, CONFIG_GREYBUS_LOG_LEVEL);

#define GB_TRANSPORT_TCPIP_BASE_PORT 4242

#ifndef CONFIG_GREYBUS_ENABLE_TLS
#define CONFIG_GREYBUS_TLS_HOSTNAME ""
#endif

/* Based on UniPro, from Linux */
#define CPORT_ID_MAX 4095

#define GB_TRANS_RX_STACK_SIZE     1024
#define GB_TRANS_RX_STACK_PRIORITY 6

#ifdef CONFIG_GREYBUS_ENABLE_TLS
DNS_SD_REGISTER_TCP_SERVICE(gb_service_advertisement, CONFIG_NET_HOSTNAME, "_greybuss", "local",
			    DNS_SD_EMPTY_TXT, GB_TRANSPORT_TCPIP_BASE_PORT);
#else  /* CONFIG_GREYBUS_ENABLE_TLS */
DNS_SD_REGISTER_TCP_SERVICE(gb_service_advertisement, CONFIG_NET_HOSTNAME, "_greybus", "local",
			    DNS_SD_EMPTY_TXT, GB_TRANSPORT_TCPIP_BASE_PORT);
#endif /* CONFIG_GREYBUS_ENABLE_TLS */

K_THREAD_STACK_DEFINE(gb_trans_rx_stack, GB_TRANS_RX_STACK_SIZE);

/*
 * struct gb_trans_ctx: Transport Context
 *
 * @rx_thread: rx_thread
 * @server_sock: socket on which the server listens for connections
 * @client_sock: socket with connection to a client
 */
struct gb_trans_ctx {
	struct k_thread rx_thread;
	int server_sock;
	int client_sock;
};

static struct gb_trans_ctx ctx;

/*
 * Helper to read data from socket
 */
static int read_data(int sock, void *data, size_t len)
{
	int ret, received = 0;

	while (received < len) {
		ret = zsock_recv(sock, received + (char *)data, len - received, 0);
		if (ret < 0) {
			LOG_ERR("Failed to receive data");
			return ret;
		} else if (ret == 0) {
			/* Socket was closed by peer */
			return 0;
		}
		received += ret;
	}
	return received;
}

/*
 * Helper to write data to socket
 */
static int write_data(int sock, const void *data, size_t len)
{
	int ret, transmitted = 0;

	while (transmitted < len) {
		ret = zsock_send(sock, transmitted + (char *)data, len - transmitted, 0);
		if (ret < 0) {
			LOG_ERR("Failed to transmit data");
			return ret;
		}
		transmitted += ret;
	}
	return transmitted;
}

/*
 * Helper to receive a greybus message from socket
 */
static struct gb_msg_with_cport gb_message_receive(int sock, bool *flag)
{
	int ret;
	struct gb_operation_msg_hdr hdr;
	struct gb_msg_with_cport msg;

	ret = read_data(sock, &msg.cport, sizeof(msg.cport));
	if (ret != sizeof(msg.cport)) {
		*flag = ret == 0;
		goto early_exit;
	}
	msg.cport = sys_le16_to_cpu(msg.cport);

	ret = read_data(sock, &hdr, sizeof(hdr));
	if (ret != sizeof(hdr)) {
		*flag = ret == 0;
		goto early_exit;
	}

	msg.msg =
		gb_message_alloc(gb_hdr_payload_len(&hdr), hdr.type, hdr.operation_id, hdr.result);
	if (!msg.msg) {
		LOG_ERR("Failed to allocate node message");
		goto early_exit;
	}

	ret = read_data(sock, (uint8_t *)msg.msg + sizeof(hdr), gb_message_payload_len(msg.msg));
	if (ret != gb_message_payload_len(msg.msg)) {
		*flag = ret == 0;
		goto free_msg;
	}

	return msg;

free_msg:
	gb_message_dealloc(msg.msg);
early_exit:
	msg.cport = 0;
	msg.msg = NULL;
	return msg;
}

static int gb_trans_listen_start(uint16_t cport)
{
	return 0;
}

static int gb_trans_listen_stop(uint16_t cport)
{
	return 0;
}

static int gb_trans_send(uint16_t cport, const struct gb_message *msg)
{
	int ret;
	__le16 cport_u16 = sys_cpu_to_le16(cport);

	if (msg->header.result) {
		LOG_INF("CPort %u, Type: %u, Result: %u, Id: %u", cport, msg->header.type,
			msg->header.result, msg->header.operation_id);
	}

	ret = write_data(ctx.client_sock, &cport_u16, sizeof(cport_u16));
	if (ret < 0) {
		return ret;
	}

	ret = write_data(ctx.client_sock, msg, sys_le16_to_cpu(msg->header.size));
	return MIN(0, ret);
}

static int netsetup()
{
	int sock, ret, family, proto = IPPROTO_TCP;
	const int yes = true;
	struct sockaddr sa;
	socklen_t sa_len;

	if (IS_ENABLED(CONFIG_GREYBUS_TLS_BUILTIN)) {
		proto = IPPROTO_TLS_1_2;
	}

	memset(&sa, 0, sizeof(sa));
	if (IS_ENABLED(CONFIG_NET_IPV6)) {
		family = AF_INET6;
		net_sin6(&sa)->sin6_family = AF_INET6;
		net_sin6(&sa)->sin6_addr = in6addr_any;
		net_sin6(&sa)->sin6_port = htons(GB_TRANSPORT_TCPIP_BASE_PORT);
		sa_len = sizeof(struct sockaddr_in6);
	} else if (IS_ENABLED(CONFIG_NET_IPV4)) {
		family = AF_INET;
		net_sin(&sa)->sin_family = AF_INET;
		net_sin(&sa)->sin_addr.s_addr = INADDR_ANY;
		net_sin(&sa)->sin_port = htons(GB_TRANSPORT_TCPIP_BASE_PORT);
		sa_len = sizeof(struct sockaddr_in);
	} else {
		LOG_ERR("Neither IPv6 nor IPv4 is available");
		return -EINVAL;
	}

	sock = zsock_socket(family, SOCK_STREAM, proto);
	if (sock < 0) {
		LOG_ERR("socket: %d", errno);
		return -errno;
	}

	ret = zsock_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	if (ret < 0) {
		LOG_ERR("setsockopt: Failed to set SO_REUSEADDR (%d)", errno);
		return -errno;
	}

	if (IS_ENABLED(CONFIG_GREYBUS_ENABLE_TLS)) {
		static const sec_tag_t sec_tag_opt[] = {
#if defined(CONFIG_GREYBUS_TLS_CLIENT_VERIFY_OPTIONAL) ||                                          \
	defined(CONFIG_GREYBUS_TLS_CLIENT_VERIFY_REQUIRED)
			GB_TLS_CA_CERT_TAG,
#endif
			GB_TLS_SERVER_CERT_TAG,
		};

		ret = zsock_setsockopt(sock, SOL_TLS, TLS_SEC_TAG_LIST, sec_tag_opt,
				       sizeof(sec_tag_opt));
		if (ret < 0) {
			LOG_ERR("setsockopt: Failed to set SEC_TAG_LIST (%d)", errno);
			return -errno;
		}

		ret = zsock_setsockopt(sock, SOL_TLS, TLS_HOSTNAME, CONFIG_GREYBUS_TLS_HOSTNAME,
				       strlen(CONFIG_GREYBUS_TLS_HOSTNAME));
		if (ret < 0) {
			LOG_ERR("setsockopt: Failed to set TLS_HOSTNAME (%d)", errno);
			return -errno;
		}

		/* default to no client verification */
		int verify = TLS_PEER_VERIFY_NONE;

		if (IS_ENABLED(CONFIG_GREYBUS_TLS_CLIENT_VERIFY_OPTIONAL)) {
			verify = TLS_PEER_VERIFY_OPTIONAL;
		}

		if (IS_ENABLED(CONFIG_GREYBUS_TLS_CLIENT_VERIFY_REQUIRED)) {
			verify = TLS_PEER_VERIFY_REQUIRED;
		}

		ret = zsock_setsockopt(sock, SOL_TLS, TLS_PEER_VERIFY, &verify, sizeof(verify));
		if (ret < 0) {
			LOG_ERR("setsockopt: Failed to set TLS_PEER_VERIFY (%d)", errno);
			return -errno;
		}
	}

	ret = zsock_bind(sock, &sa, sa_len);
	if (ret < 0) {
		LOG_ERR("bind: %d", errno);
		return -errno;
	}

	/* We will only ever be connected to a single ap */
	ret = zsock_listen(sock, 1);
	if (ret < 0) {
		LOG_ERR("listen: %d", errno);
		return -errno;
	}

	LOG_INF("Greybus socket opened at port %zu", htons(GB_TRANSPORT_TCPIP_BASE_PORT));

	return sock;
}

/*
 * Helper to accept new connection
 */
static void gb_trans_accept(struct gb_trans_ctx *ctx)
{
	int ret;
	struct zsock_pollfd fd = {
		.fd = ctx->server_sock,
		.events = ZSOCK_POLLIN,
	};
	struct sockaddr_in6 addr = {
		.sin6_family = AF_INET6,
		.sin6_addr = in6addr_any,
	};
	socklen_t addrlen = sizeof(addr);

	ret = zsock_poll(&fd, 1, -1);
	if (ret < 0) {
		LOG_ERR("Socket poll failed");
		return;
	}

	if (fd.revents & ZSOCK_POLLIN) {
		ret = zsock_accept(fd.fd, (struct sockaddr *)&addr, &addrlen);
		if (ret < 0) {
			LOG_ERR("Failed to accept connection");
			return;
		}
		ctx->client_sock = ret;
	}

	LOG_INF("Accepted new connection");
}

/*
 * Helper to receive messages if socket connection is established
 */
static void gb_trans_rx(struct gb_trans_ctx *ctx)
{
	int ret;
	bool flag = false;
	struct gb_msg_with_cport msg;
	struct zsock_pollfd fd = {
		.fd = ctx->client_sock,
		.events = ZSOCK_POLLIN,
	};

	ret = zsock_poll(&fd, 1, -1);
	if (ret < 0) {
		LOG_ERR("Socket poll failed");
		return;
	}

	if (fd.revents & ZSOCK_POLLIN) {
		msg = gb_message_receive(fd.fd, &flag);
		if (flag) {
			zsock_close(fd.fd);
			ctx->client_sock = -1;
			return;
		}

		if (!msg.msg) {
			LOG_ERR("Failed to receive message");
			return;
		}

		ret = greybus_rx_handler(msg.cport, msg.msg);
		if (ret < 0) {
			LOG_ERR("Failed to receive greybus message");
			gb_message_dealloc(msg.msg);
		}
	}
}

/*
 * Hander function for rx thread
 */
static void gb_trans_rx_thread_handler(void *p1, void *p2, void *p3)
{
	while (true) {
		if (ctx.client_sock == -1) {
			gb_trans_accept(&ctx);
		} else {
			gb_trans_rx(&ctx);
		}
	}
}

static int gb_trans_init(void)
{
	ctx.server_sock = netsetup();

	if (ctx.server_sock < 0) {
		LOG_ERR("Failed to setup base TCP port");
		return -ESOCKTNOSUPPORT;
	}
	ctx.client_sock = -1;

	k_thread_create(&ctx.rx_thread, gb_trans_rx_stack, K_THREAD_STACK_SIZEOF(gb_trans_rx_stack),
			gb_trans_rx_thread_handler, NULL, NULL, NULL, GB_TRANS_RX_STACK_PRIORITY, 0,
			K_NO_WAIT);

	return 0;
}

static void gb_trans_exit(void)
{
	k_thread_abort(&ctx.rx_thread);
	zsock_close(ctx.server_sock);
	zsock_close(ctx.client_sock);
}

const struct gb_transport_backend gb_trans_backend = {
	.init = gb_trans_init,
	.exit = gb_trans_exit,
	.listen = gb_trans_listen_start,
	.stop_listening = gb_trans_listen_stop,
	.send = gb_trans_send,
};
