/*
 * Copyright (c) 2024, Jamie McCrae
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/lorawan/lorawan.h>
#include <zephyr/mgmt/mcumgr/smp/smp.h>
#include <zephyr/mgmt/mcumgr/transport/smp.h>
#include <zephyr/mgmt/mcumgr/mgmt/handlers.h>

#include <mgmt/mcumgr/transport/smp_internal.h>
#include <mgmt/mcumgr/transport/smp_reassembly.h>

#ifdef CONFIG_MCUMGR_GRP_TRANSPORT
#include <zephyr/mgmt/mcumgr/grp/transport_mgmt/transport_mgmt.h>
#endif

LOG_MODULE_REGISTER(smp_lorawan, CONFIG_MCUMGR_TRANSPORT_LORAWAN_LOG_LEVEL);

static void smp_lorawan_downlink(uint8_t port, uint8_t flags, int16_t rssi, int8_t snr,
				 uint8_t len, const uint8_t *hex_data);

static int smp_lorawan_uplink(struct net_buf *nb);

static uint16_t smp_lorawan_get_mtu(const struct net_buf *nb);

#ifdef CONFIG_MCUMGR_GRP_TRANSPORT
static bool smp_lorawan_bridge_connect(struct smp_transport_bridge *bridge, bool outgoing,
				       uint32_t mode, bool same_transport,
				       zcbor_state_t *input_data, zcbor_state_t *output_data);

static void smp_lorawan_bridge_disconnect(struct smp_transport_bridge *bridge, bool outgoing);

static int smp_lorawan_bridge_tx(const struct smp_transport_bridge *bridge, struct net_buf *nb,
				 bool outgoing);

static bool smp_lorawan_bridge_modes(zcbor_state_t *output_data, int *rc)

static bool smp_lorawan_bridge_config_details(uint32_t mode, zcbor_state_t *output_data, int *rc);
#endif

static struct lorawan_downlink_cb lorawan_smp_downlink_cb = {
	.port = CONFIG_MCUMGR_TRANSPORT_LORAWAN_FRAME_PORT,
	.cb = smp_lorawan_downlink,
};

struct smp_transport smp_lorawan_transport = {
	.functions.output = smp_lorawan_uplink,
	.functions.get_mtu = smp_lorawan_get_mtu,
#ifdef CONFIG_MCUMGR_GRP_TRANSPORT
	.functions.bridge_connect = smp_lorawan_bridge_connect,
	.functions.bridge_disconnect = smp_lorawan_bridge_disconnect,
	.functions.bridge_output = smp_lorawan_bridge_tx,
#if defined(CONFIG_MCUMGR_GRP_TRANSPORT_INFO_FUNCTIONS)
	.functions.bridge_modes = smp_lorawan_bridge_modes,
	.functions.bridge_config_details = smp_lorawan_bridge_config_details,
#endif
#endif
};

#if defined(CONFIG_SMP_CLIENT) || defined(CONFIG_MCUMGR_GRP_TRANSPORT)
struct smp_client_transport_entry smp_lorawan_client_transport = {
	.smpt = &smp_lorawan_transport,
	.smpt_type = SMP_LORAWAN_TRANSPORT,
#ifdef CONFIG_MCUMGR_GRP_TRANSPORT_INFO_FUNCTIONS
	.name = "LoRaWAN",
#endif
};
#endif

#ifdef CONFIG_MCUMGR_TRANSPORT_LORAWAN_POLL_FOR_DATA
static struct k_thread smp_lorawan_thread;
K_KERNEL_STACK_MEMBER(smp_lorawan_stack, CONFIG_MCUMGR_TRANSPORT_LORAWAN_POLL_FOR_DATA_STACK_SIZE);
K_FIFO_DEFINE(smp_lorawan_fifo);

struct smp_lorawan_uplink_message_t {
	void *fifo_reserved;
	struct net_buf *nb;
	struct k_sem my_sem;
};

static struct smp_lorawan_uplink_message_t empty_message = {
	.nb = NULL,
};

static void smp_lorawan_uplink_thread(void *p1, void *p2, void *p3)
{
	struct smp_lorawan_uplink_message_t *msg;

	while (1) {
		msg = k_fifo_get(&smp_lorawan_fifo, K_FOREVER);
		uint16_t size = 0;
		uint16_t pos = 0;

		if (msg->nb != NULL) {
			size = msg->nb->len;
		}

		while (pos < size || size == 0) {
			uint8_t *data = NULL;
			uint8_t data_size;
			uint8_t temp;
			uint8_t tries = CONFIG_MCUMGR_TRANSPORT_LORAWAN_POLL_FOR_DATA_RETRIES;

			lorawan_get_payload_sizes(&data_size, &temp);

			if (data_size > size) {
				data_size = size;
			}

			if (size > 0) {
				if ((data_size + pos) > size) {
					data_size = size - pos;
				}

				data = net_buf_pull_mem(msg->nb, data_size);
			}

			while (tries > 0) {
				int rc;

				rc = lorawan_send(CONFIG_MCUMGR_TRANSPORT_LORAWAN_FRAME_PORT,
						  data, data_size,
#if defined(CONFIG_MCUMGR_TRANSPORT_LORAWAN_CONFIRMED_UPLINKS)
						  LORAWAN_MSG_CONFIRMED
#else
						  LORAWAN_MSG_UNCONFIRMED
#endif
						 );

				if (rc != 0) {
					--tries;
				} else {
					break;
				}
			}

			if (size == 0) {
				break;
			}

			pos += data_size;
		}

		/* For empty packets, do not trigger semaphore */
		if (size != 0) {
			k_sem_give(&msg->my_sem);
		}
	}
}
#endif

static void smp_lorawan_downlink(uint8_t port, uint8_t flags, int16_t rssi, int8_t snr,
				 uint8_t len, const uint8_t *hex_data)
{
	ARG_UNUSED(flags);
	ARG_UNUSED(rssi);
	ARG_UNUSED(snr);

	if (port == CONFIG_MCUMGR_TRANSPORT_LORAWAN_FRAME_PORT) {
#ifdef CONFIG_MCUMGR_TRANSPORT_LORAWAN_REASSEMBLY
		int rc;

		if (len == 0) {
			/* Empty packet is used to clear partially queued data */
			(void)smp_reassembly_drop(&smp_lorawan_transport);
		} else {
			rc = smp_reassembly_collect(&smp_lorawan_transport, hex_data, len);

			if (rc == 0) {
				rc = smp_reassembly_complete(&smp_lorawan_transport, false);

				if (rc) {
					LOG_ERR("LoRaWAN SMP reassembly complete failed: %d", rc);
				}
			} else if (rc < 0) {
				LOG_ERR("LoRaWAN SMP reassembly collect failed: %d", rc);
			} else {
				LOG_ERR("LoRaWAN SMP expected data left: %d", rc);

#ifdef CONFIG_MCUMGR_TRANSPORT_LORAWAN_POLL_FOR_DATA
				/* Send empty LoRaWAN packet to receive next packet from server */
				k_fifo_put(&smp_lorawan_fifo, &empty_message);
#endif
			}
		}
#else
		if (len > sizeof(struct smp_hdr)) {
			struct net_buf *nb;

			nb = smp_packet_alloc();

			if (!nb) {
				LOG_ERR("LoRaWAN SMP packet allocation failure");
				return;
			}

			net_buf_add_mem(nb, hex_data, len);
			smp_rx_req(&smp_lorawan_transport, nb);
		} else {
			LOG_ERR("Invalid LoRaWAN SMP downlink");
		}
#endif
	} else {
		LOG_ERR("Invalid LoRaWAN SMP downlink");
	}
}

static int smp_lorawan_uplink(struct net_buf *nb)
{
	int rc = 0;

#ifdef CONFIG_MCUMGR_TRANSPORT_LORAWAN_FRAGMENTED_UPLINKS
	struct smp_lorawan_uplink_message_t tx_data = {
		.nb = nb,
	};

	k_sem_init(&tx_data.my_sem, 0, 1);
	k_fifo_put(&smp_lorawan_fifo, &tx_data);
	k_sem_take(&tx_data.my_sem, K_FOREVER);
#else
	uint8_t data_size;
	uint8_t temp;

	lorawan_get_payload_sizes(&data_size, &temp);

	if (nb->len > data_size) {
		LOG_ERR("Cannot send LoRaWAN SMP message, too large. Message: %d, maximum: %d",
			nb->len, data_size);
	} else {
		rc = lorawan_send(CONFIG_MCUMGR_TRANSPORT_LORAWAN_FRAME_PORT, nb->data, nb->len,
#if defined(CONFIG_MCUMGR_TRANSPORT_LORAWAN_CONFIRMED_UPLINKS)
				  LORAWAN_MSG_CONFIRMED
#else
				  LORAWAN_MSG_UNCONFIRMED
#endif
				 );

		if (rc != 0) {
			LOG_ERR("Failed to send LoRaWAN SMP message: %d", rc);
		}
	}
#endif

	smp_packet_free(nb);

	return rc;
}

static uint16_t smp_lorawan_get_mtu(const struct net_buf *nb)
{
	ARG_UNUSED(nb);

	uint8_t max_data_size;
	uint8_t temp;

	lorawan_get_payload_sizes(&max_data_size, &temp);

	return (uint16_t)max_data_size;
}

#ifdef CONFIG_MCUMGR_GRP_TRANSPORT
static bool smp_lorawan_bridge_connect(struct smp_transport_bridge *bridge, bool outgoing,
				       uint32_t mode, bool same_transport,
				       zcbor_state_t *input_data, zcbor_state_t *output_data)
{
	ARG_UNUSED(bridge);
	ARG_UNUSED(outgoing);
	ARG_UNUSED(input_data);
	ARG_UNUSED(output_data);

	if (mode != 0) {
		smp_add_cmd_err(output_data, MGMT_GROUP_ID_TRANSPORT,
				TRANSPORT_MGMT_ERR_INVALID_MODE);

		return false;
	}

	if (same_transport == true) {
		smp_add_cmd_err(output_data, MGMT_GROUP_ID_TRANSPORT,
				TRANSPORT_MGMT_ERR_SAME_BRIDGE_DEVICE_DISALLOWED);
		return false;
	}

	if (outgoing == true) {
		smp_add_cmd_err(output_data, MGMT_GROUP_ID_TRANSPORT,
				TRANSPORT_MGMT_ERR_TRASNPORT_OUTGOING_NOT_SUPPORTED);
		return false;
	}

	return true;
}

static void smp_lorawan_bridge_disconnect(struct smp_transport_bridge *bridge, bool outgoing)
{
	ARG_UNUSED(bridge);
	ARG_UNUSED(outgoing);
}

static int smp_lorawan_bridge_tx(const struct smp_transport_bridge *bridge, struct net_buf *nb,
				 bool outgoing)
{
	ARG_UNUSED(bridge);
	ARG_UNUSED(outgoing);

	return smp_lorawan_uplink(nb);
}

#if defined(CONFIG_MCUMGR_GRP_TRANSPORT_INFO_FUNCTIONS)
static bool smp_lorawan_bridge_modes(zcbor_state_t *output_data, int *rc)
{
	bool ok;

	ok = zcbor_map_start_encode(output_data, 2) &&
	     zcbor_tstr_put_lit(output_data, "id") &&
	     zcbor_uint32_put(output_data, 0) &&
	     zcbor_tstr_put_lit(output_data, "description") &&
	     zcbor_tstr_put_lit(output_data, "LoRaWAN") &&
	     zcbor_tstr_put_lit(output_data, "incoming") &&
	     zcbor_bool_put(output_data, true) &&
	     zcbor_map_end_encode(output_data, 2);

	*rc = MGMT_RETURN_CHECK(ok);
	return ok;
}

static bool smp_lorawan_bridge_config_details(uint32_t mode, zcbor_state_t *output_data, int *rc)
{
	if (mode == 0) {
		return true;
	}

	smp_mgmt_reset_zse_writer(output_data);
	smp_add_cmd_err(output_data, MGMT_GROUP_ID_TRANSPORT, TRANSPORT_MGMT_ERR_INVALID_MODE);
	*rc = 0;

	return false;
}
#endif
#endif

static void smp_lorawan_start(void)
{
	int rc;

	rc = smp_transport_init(&smp_lorawan_transport);

#if defined(CONFIG_SMP_CLIENT) || defined(CONFIG_MCUMGR_GRP_TRANSPORT)
	if (rc == 0) {
		smp_client_transport_register(&smp_lorawan_client_transport);
	}
#endif

	if (rc == 0) {
		lorawan_register_downlink_callback(&lorawan_smp_downlink_cb);
	} else {
		LOG_ERR("Failed to init LoRaWAN MCUmgr SMP transport: %d", rc);
	}

#ifdef CONFIG_MCUMGR_TRANSPORT_LORAWAN_POLL_FOR_DATA
	k_thread_create(&smp_lorawan_thread, smp_lorawan_stack,
			K_KERNEL_STACK_SIZEOF(smp_lorawan_stack),
			smp_lorawan_uplink_thread, NULL, NULL, NULL,
			CONFIG_MCUMGR_TRANSPORT_LORAWAN_POLL_FOR_DATA_THREAD_PRIORITY, 0,
			K_FOREVER);

	k_thread_start(&smp_lorawan_thread);
#endif
}

MCUMGR_HANDLER_DEFINE(smp_lorawan, smp_lorawan_start);
