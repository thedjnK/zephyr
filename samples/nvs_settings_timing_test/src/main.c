/*
 * Copyright (c) 2024, Jamie M.
 *
 * All right reserved. This code is NOT apache or FOSS/copyleft licensed.
 */

#include <zephyr/kernel.h>
#include <stdio.h>
#include "settings.h"
#include <zephyr/settings/settings.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, 4);

uint8_t dummy_dev_eui[LORA_DEV_EUI_SIZE] = {0x96, 0xac, 0xba, 0x35, 0x60, 0x26, 0xad, 0x1c};
uint8_t dummy_join_eui[LORA_JOIN_EUI_SIZE] = {0xf5, 0x26, 0x14, 0xf8, 0x54, 0xfc, 0x0e, 0x9c};
uint8_t dummy_app_key[LORA_APP_KEY_SIZE] = {0x41, 0x04, 0x5b, 0x8b, 0x86, 0x48, 0x29, 0xe0, 0xe1, 0xc0, 0x73, 0x71, 0xb8, 0x38, 0xa1, 0x05};
uint8_t dummy_power_offset[POWER_OFFSET_MV_SIZE] = {0x00, 0x88};
uint8_t dummy_name[] = "lol fake";
uint8_t dummy_passkey[BLUETOOTH_FIXED_PASSKEY_SIZE] = {0x34, 0x12, 0x00, 0x00};
uint32_t dummy_num = 0;

int main(void)
{
	int rc;
	uint8_t read_dev_eui[LORA_DEV_EUI_SIZE];
	uint8_t read_join_eui[LORA_JOIN_EUI_SIZE];
	uint8_t read_app_key[LORA_APP_KEY_SIZE];
	uint8_t read_power_offset[POWER_OFFSET_MV_SIZE];
	uint8_t read_name[20];
	uint8_t read_passkey[BLUETOOTH_FIXED_PASSKEY_SIZE];
	uint32_t read_num;

	settings_load();
	lora_keys_load();
	app_keys_load();
	other_keys_load();

	//Set dummy keys
	rc = settings_runtime_set("lora_keys/dev_eui", dummy_dev_eui, sizeof(dummy_dev_eui));

	if (rc) {
		LOG_ERR("set1 fail: %d", rc);
	}

	rc = settings_runtime_set("lora_keys/join_eui", dummy_join_eui, sizeof(dummy_join_eui));

	if (rc) {
		LOG_ERR("set2 fail: %d", rc);
	}

	rc = settings_runtime_set("lora_keys/app_key", dummy_app_key, sizeof(dummy_app_key));

	if (rc) {
		LOG_ERR("set3 fail: %d", rc);
	}

	rc = settings_runtime_set("app/power_offset", dummy_power_offset, sizeof(dummy_power_offset));

	if (rc) {
		LOG_ERR("set4 fail: %d", rc);
	}

	rc = settings_runtime_set("app/bluetooth_name", dummy_name, sizeof(dummy_name));

	if (rc) {
		LOG_ERR("set5 fail: %d", rc);
	}

	rc = settings_runtime_set("app/bluetooth_fixed_passkey", dummy_passkey, sizeof(dummy_passkey));

	if (rc) {
		LOG_ERR("set6 fail: %d", rc);
	}

	//Get default
	rc = settings_runtime_get("other/num", &dummy_num, sizeof(dummy_num));

	if (rc < 0) {
		LOG_ERR("get1 fail: %d", rc);
	}

	uint32_t loop = 1;

	while (1) {
		//Clear
		uint32_t uptime_start;
		uint32_t uptime_end;
		LOG_ERR("Loop %d...", loop);
		uptime_start = k_uptime_get_32();
		memset(read_dev_eui, 0, LORA_DEV_EUI_SIZE);
		memset(read_join_eui, 0, LORA_JOIN_EUI_SIZE);
		memset(read_app_key, 0, LORA_APP_KEY_SIZE);
		memset(read_power_offset, 0, POWER_OFFSET_MV_SIZE);
		memset(read_name, 0, 20);
		memset(read_passkey, 0, BLUETOOTH_FIXED_PASSKEY_SIZE);
		read_num = 0;

		//Get
		rc = settings_runtime_get("other/num", &read_num, sizeof(read_num));

		if (rc != sizeof(read_num)) {
			LOG_ERR("get2 fail: %d", rc);
		}

		if (read_num != dummy_num) {
			LOG_ERR("num mistmatch, expected %d got %d, rc: %d", dummy_num, read_num, rc);
		}

		//Update
		++dummy_num;
		rc = settings_runtime_set("other/num", &dummy_num, sizeof(dummy_num));

		if (rc < 0) {
			LOG_ERR("set7 fail: %d", rc);
		}

		//Fetch others
		rc = settings_runtime_get("other/num", &read_num, sizeof(read_num));

		if (rc != sizeof(read_num)) {
			LOG_ERR("get3 fail: %d", rc);
		}

		rc = settings_runtime_get("lora_keys/dev_eui", read_dev_eui, sizeof(read_dev_eui));

		if (rc != sizeof(read_dev_eui)) {
			LOG_ERR("get4 fail: %d", rc);
		}

		rc = settings_runtime_get("lora_keys/join_eui", read_join_eui, sizeof(read_join_eui));

		if (rc != sizeof(read_join_eui)) {
			LOG_ERR("get5 fail: %d", rc);
		}

		rc = settings_runtime_get("lora_keys/app_key", read_app_key, sizeof(read_app_key));

		if (rc != sizeof(read_app_key)) {
			LOG_ERR("get6 fail: %d", rc);
		}

		rc = settings_runtime_get("app/power_offset", read_power_offset, sizeof(read_power_offset));

		if (rc != sizeof(read_power_offset)) {
			LOG_ERR("get7 fail: %d", rc);
		}

		rc = settings_runtime_get("app/bluetooth_name", read_name, sizeof(read_name));

		if (rc < 3) {
			LOG_ERR("get8 fail: %d", rc);
		}

		rc = settings_runtime_get("app/bluetooth_fixed_passkey", read_passkey, sizeof(read_passkey));

		if (rc != sizeof(read_passkey)) {
			LOG_ERR("get9 fail: %d", rc);
		}

		//Compare data
		if (memcmp(dummy_dev_eui, read_dev_eui, sizeof(read_dev_eui)) != 0) {
			LOG_ERR("cmp1 fail");
			LOG_HEXDUMP_ERR(dummy_dev_eui, sizeof(read_dev_eui), "expected");
			LOG_HEXDUMP_ERR(read_dev_eui, sizeof(read_dev_eui), "got");
		}

		if (memcmp(dummy_join_eui, read_join_eui, sizeof(read_join_eui)) != 0) {
			LOG_ERR("cmp2 fail");
			LOG_HEXDUMP_ERR(dummy_join_eui, sizeof(read_join_eui), "expected");
			LOG_HEXDUMP_ERR(read_join_eui, sizeof(read_join_eui), "got");
		}

		if (memcmp(dummy_app_key, read_app_key, sizeof(read_app_key)) != 0) {
			LOG_ERR("cmp3 fail");
			LOG_HEXDUMP_ERR(dummy_app_key, sizeof(read_app_key), "expected");
			LOG_HEXDUMP_ERR(read_app_key, sizeof(read_app_key), "got");
		}

		if (memcmp(dummy_power_offset, read_power_offset, sizeof(read_power_offset)) != 0) {
			LOG_ERR("cmp4 fail");
			LOG_HEXDUMP_ERR(dummy_power_offset, sizeof(read_power_offset), "expected");
			LOG_HEXDUMP_ERR(read_power_offset, sizeof(read_power_offset), "got");
		}

		if (memcmp(dummy_name, read_name, strlen(dummy_name)) != 0) {
			LOG_ERR("cmp5 fail");
			LOG_HEXDUMP_ERR(dummy_name, sizeof(read_name), "expected");
			LOG_HEXDUMP_ERR(read_name, sizeof(read_name), "got");
		}

		if (memcmp(dummy_passkey, read_passkey, sizeof(read_passkey)) != 0) {
			LOG_ERR("cmp6 fail");
			LOG_HEXDUMP_ERR(dummy_passkey, sizeof(read_passkey), "expected");
			LOG_HEXDUMP_ERR(read_passkey, sizeof(read_passkey), "got");
		}

		if (read_num != dummy_num) {
			LOG_ERR("cmp7 fail, expected %d got %d", dummy_num, read_num);
		}

		uptime_end = k_uptime_get_32();
		LOG_ERR("Finished in %dms", (uptime_end - uptime_start));

		k_sleep(K_MSEC(350));
		++loop;
	}

	return 0;
}
