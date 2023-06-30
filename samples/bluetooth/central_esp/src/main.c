/*
 * Copyright (c) 2023 Jamie M.
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(abe, 4);

enum device_state_t {
	STATE_IDLE = 0,
	STATE_CONNECTING,
	STATE_CONNECTED,
	STATE_DISCOVERING,
	STATE_ACTIVE,
};

enum handle_status_t {
	FIND_ESS_SERVICE = 0,
	FIND_PRESSURE,
	FIND_PRESSURE_CCC,
	FIND_HUMIDITY,
	FIND_HUMIDITY_CCC,
	FIND_DEW_POINT,
	FIND_DEW_POINT_CCC,
	FIND_TEMPERATURE,
	FIND_TEMPERATURE_CCC,
	SUBSCRIBE_PRESSURE,
	SUBSCRIBE_HUMDIITY,
	SUBSCRIBE_DEW_POINT,
	SUBSCRIBE_TEMPERATURE,
};

struct device_handles {
	enum handle_status_t status;
	uint16_t service;
	struct bt_gatt_subscribe_params temperature;
	struct bt_gatt_subscribe_params pressure;
	struct bt_gatt_subscribe_params humidity;
	struct bt_gatt_subscribe_params dew_point;
};

struct device_params {
	bt_addr_le_t address;
	enum device_state_t state;
	struct bt_conn *connection;
	struct device_handles handles;
	char *name;
};

struct device_params devices[2] = {
	{
		.address = {
			.type = BT_ADDR_LE_RANDOM,
			.a.val = { 0xc5, 0x2a, 0xc2, 0x37, 0x3e, 0xe2 },
		},
		.name = "first",
	},
	{
		.address = {
			.type = BT_ADDR_LE_RANDOM,
			.a.val = { 0x22, 0x07, 0x7b, 0x1c, 0xb2, 0xf7 },
		},
		.name = "second",
	}
};

#define DEVICE_COUNT ARRAY_SIZE(devices)
static uint8_t current_index = 0;

static struct bt_uuid_16 uuid = BT_UUID_INIT_16(0);
static struct bt_gatt_discover_params discover_params;

static uint8_t notify_func(struct bt_conn *conn,
			   struct bt_gatt_subscribe_params *params,
			   const void *data, uint16_t length)
{
	uint8_t i = 0;

	if (!data) {
		LOG_ERR("[UNSUBSCRIBED]");
		params->value_handle = 0U;
		return BT_GATT_ITER_STOP;
	}

	while (i < DEVICE_COUNT) {
		if (devices[i].connection == conn) {
			break;
		}
		++i;
	}

	if (i == DEVICE_COUNT) {
		LOG_ERR("ERROR! INVALID CONNECTION!");
		return BT_GATT_ITER_STOP;
	}

	LOG_ERR("[NOTIFICATION] data %p length %u", data, length);
//	LOG_HEXDUMP_ERR(data, length, "Value");

	if (params == &devices[i].handles.temperature) {
		uint16_t value;
		float fp_value;

		value = sys_get_le16(&((uint8_t *)data)[0]);
		fp_value = ((float)value) / 100.0;
LOG_ERR("temp = %fc", fp_value);
//2?
	} else if (params == &devices[i].handles.pressure) {
		uint32_t value;
		float fp_value;

		value = sys_get_le32(&((uint8_t *)data)[0]);
		fp_value = (float)value;
LOG_ERR("press = %fPa", fp_value);
//4?
	} else if (params == &devices[i].handles.humidity) {
		uint16_t value;
		float fp_value;

		value = sys_get_le16(&((uint8_t *)data)[0]);
		fp_value = ((float)value) / 100.0;
LOG_ERR("hum = %f\\%", fp_value);
//2?
	} else if (params == &devices[i].handles.dew_point) {
LOG_ERR("dew = %dc", ((uint8_t *)data)[0]);
//1?
	} else {
LOG_ERR("not valid");
	}

	return BT_GATT_ITER_CONTINUE;
}

static struct k_work subscribe_workqueue;
void subscribe_func(struct bt_conn *conn, uint8_t err, struct bt_gatt_subscribe_params *params);

static void subscribe_work(struct k_work *work)
{
	int err;
	struct bt_gatt_subscribe_params *params;

	if (devices[current_index].handles.status == SUBSCRIBE_PRESSURE) {
		devices[current_index].handles.status = SUBSCRIBE_HUMDIITY;
		params = &devices[current_index].handles.humidity;
	} else if (devices[current_index].handles.status == SUBSCRIBE_HUMDIITY) {
		devices[current_index].handles.status = SUBSCRIBE_DEW_POINT;
		params = &devices[current_index].handles.dew_point;
	} else if (devices[current_index].handles.status == SUBSCRIBE_DEW_POINT) {
		devices[current_index].handles.status = SUBSCRIBE_TEMPERATURE;
		params = &devices[current_index].handles.temperature;
	} else {
		return;
	}

/* */
	params->subscribe = subscribe_func;
	params->write = NULL;
	params->notify = notify_func;
	params->value = BT_GATT_CCC_NOTIFY;

	err = bt_gatt_subscribe(devices[current_index].connection, params);
	if (err && err != -EALREADY) {
		LOG_ERR("Subscribe failed (err %d)", err);
	} else {
		LOG_ERR("[SUBSCRIBED]");
	}
}

void subscribe_func(struct bt_conn *conn, uint8_t err, struct bt_gatt_subscribe_params *params)
{
	if (err != 0) {
		LOG_ERR("Gonna matey");
		return;
	}

	if (devices[current_index].handles.status == SUBSCRIBE_TEMPERATURE) {
		LOG_ERR("All finished!");
		return;
	} else {
	        k_work_submit(&subscribe_workqueue);
	}
}

static uint8_t discover_func(struct bt_conn *conn,
			     const struct bt_gatt_attr *attr,
			     struct bt_gatt_discover_params *params)
{
	int err;

	if (!attr) {
		LOG_ERR("Discover complete");
		(void)memset(params, 0, sizeof(*params));
		return BT_GATT_ITER_STOP;
	}

	LOG_ERR("[ATTRIBUTE] handle %u", attr->handle);

	if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_ESS)) {
		devices[current_index].handles.status = FIND_PRESSURE;
		devices[current_index].handles.service = attr->handle;

		memcpy(&uuid, BT_UUID_PRESSURE, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.start_handle = attr->handle + 1;
		discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			LOG_ERR("Discover failed (err %d)", err);
		}
	} else if (!bt_uuid_cmp(discover_params.uuid,
				BT_UUID_PRESSURE)) {
		devices[current_index].handles.status = FIND_PRESSURE_CCC;
//		devices[current_index].handles.pressure = attr->handle;
//		devices[current_index].handles.pressure_value = bt_gatt_attr_value_handle(attr);
		devices[current_index].handles.pressure.value_handle = bt_gatt_attr_value_handle(attr);

		memcpy(&uuid, BT_UUID_GATT_CCC, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.start_handle = attr->handle + 2;
		discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
//		subscribe_params.value_handle = bt_gatt_attr_value_handle(attr);

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			LOG_ERR("Discover failed (err %d)", err);
		}
	} else if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_GATT_CCC) && devices[current_index].handles.status == FIND_PRESSURE_CCC) {
		devices[current_index].handles.status = FIND_HUMIDITY;
//		devices[current_index].handles.pressure_ccc = attr->handle;
		devices[current_index].handles.pressure.ccc_handle = attr->handle;

		memcpy(&uuid, BT_UUID_HUMIDITY, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.start_handle = devices[current_index].handles.service + 1;
		discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			LOG_ERR("Discover failed (err %d)", err);
		}
	} else if (!bt_uuid_cmp(discover_params.uuid,
				BT_UUID_HUMIDITY)) {
		devices[current_index].handles.status = FIND_HUMIDITY_CCC;
//		devices[current_index].handles.humidity = attr->handle;
//		devices[current_index].handles.humidity_value = bt_gatt_attr_value_handle(attr);
		devices[current_index].handles.humidity.value_handle = bt_gatt_attr_value_handle(attr);

		memcpy(&uuid, BT_UUID_GATT_CCC, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.start_handle = attr->handle + 2;
		discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
//		subscribe_params.value_handle = bt_gatt_attr_value_handle(attr);

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			LOG_ERR("Discover failed (err %d)", err);
		}
	} else if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_GATT_CCC) && devices[current_index].handles.status == FIND_HUMIDITY_CCC) {
		devices[current_index].handles.status = FIND_DEW_POINT;
//		devices[current_index].handles.humidity_ccc = attr->handle;
		devices[current_index].handles.humidity.ccc_handle = attr->handle;

		memcpy(&uuid, BT_UUID_DEW_POINT, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.start_handle = devices[current_index].handles.service + 1;
		discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			LOG_ERR("Discover failed (err %d)", err);
		}
	} else if (!bt_uuid_cmp(discover_params.uuid,
				BT_UUID_DEW_POINT)) {
		devices[current_index].handles.status = FIND_DEW_POINT_CCC;
//		devices[current_index].handles.dew_point = attr->handle;
//		devices[current_index].handles.dew_point_value = bt_gatt_attr_value_handle(attr);
		devices[current_index].handles.dew_point.value_handle = bt_gatt_attr_value_handle(attr);

		memcpy(&uuid, BT_UUID_GATT_CCC, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.start_handle = attr->handle + 2;
		discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
//		subscribe_params.value_handle = bt_gatt_attr_value_handle(attr);

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			LOG_ERR("Discover failed (err %d)", err);
		}
	} else if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_GATT_CCC) && devices[current_index].handles.status == FIND_DEW_POINT_CCC) {
		devices[current_index].handles.status = FIND_TEMPERATURE;
//		devices[current_index].handles.dew_point_ccc = attr->handle;
		devices[current_index].handles.dew_point.ccc_handle = attr->handle;

		memcpy(&uuid, BT_UUID_TEMPERATURE, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.start_handle = devices[current_index].handles.service + 1;
		discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			LOG_ERR("Discover failed (err %d)", err);
		}
	} else if (!bt_uuid_cmp(discover_params.uuid,
				BT_UUID_TEMPERATURE)) {
		devices[current_index].handles.status = FIND_TEMPERATURE_CCC;
//		devices[current_index].handles.temperature = attr->handle;
//		devices[current_index].handles.temperature_value = bt_gatt_attr_value_handle(attr);
		devices[current_index].handles.temperature.value_handle = bt_gatt_attr_value_handle(attr);

		memcpy(&uuid, BT_UUID_GATT_CCC, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.start_handle = attr->handle + 2;
		discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
//		subscribe_params.value_handle = bt_gatt_attr_value_handle(attr);

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			LOG_ERR("Discover failed (err %d)", err);
		}
	} else if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_GATT_CCC) && devices[current_index].handles.status == FIND_TEMPERATURE_CCC) {
		devices[current_index].handles.status = SUBSCRIBE_PRESSURE;
//		devices[current_index].handles.temperature_ccc = attr->handle;
		devices[current_index].handles.temperature.ccc_handle = attr->handle;

		devices[current_index].handles.pressure.subscribe = subscribe_func;
		devices[current_index].handles.pressure.write = NULL;
		devices[current_index].handles.pressure.notify = notify_func;
		devices[current_index].handles.pressure.value = BT_GATT_CCC_NOTIFY;

		err = bt_gatt_subscribe(conn, &devices[current_index].handles.pressure);
		if (err && err != -EALREADY) {
			LOG_ERR("Subscribe failed (err %d)", err);
		} else {
			LOG_ERR("[SUBSCRIBED]");
		}

		return BT_GATT_ITER_STOP;
	}

	return BT_GATT_ITER_STOP;
}

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (conn_err) {
		LOG_ERR("Failed to connect to %s (%u)", addr, conn_err);

		bt_conn_unref(conn);

		devices[current_index].state = STATE_IDLE;
		return;
	}

	devices[current_index].state = STATE_CONNECTED;
	memset(&devices[current_index].handles, 0, sizeof(struct device_handles));

	LOG_ERR("Connected: %s", addr);

	memcpy(&uuid, BT_UUID_ESS, sizeof(uuid));
	devices[current_index].handles.status = FIND_ESS_SERVICE;

	discover_params.uuid = &uuid.uuid;
	discover_params.func = discover_func;
	discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
	discover_params.type = BT_GATT_DISCOVER_PRIMARY;

	err = bt_gatt_discover(conn, &discover_params);
	if (err) {
		LOG_ERR("Discover failed(err %d)", err);
		return;
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_ERR("Disconnected: %s (reason 0x%02x)", addr, reason);

	bt_conn_unref(conn);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

int main(void)
{
	int err;
	err = bt_enable(NULL);

	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return 0;
	}

	LOG_ERR("Bluetooth initialized");

        k_work_init(&subscribe_workqueue, subscribe_work);

	struct bt_le_conn_param *param = BT_LE_CONN_PARAM_DEFAULT;
	devices[current_index].state = STATE_CONNECTING;

	err = bt_conn_le_create(&devices[current_index].address, BT_CONN_LE_CREATE_CONN, param, &devices[current_index].connection);

	return 0;
}
