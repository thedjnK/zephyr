/*
 * Copyright (c) 2023 Jamie McCrae
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/auxdisplay.h>
#include <zephyr/logging/log.h>

#ifdef CONFIG_PM_DEVICE
#include <zephyr/pm/device.h>
#endif

LOG_MODULE_REGISTER(auxdisplay_sample, LOG_LEVEL_DBG);

void main(void)
{
	int rc;
	const struct device *const dev = DEVICE_DT_GET(DT_NODELABEL(auxdisplay_0));
	uint8_t data[64];

	if (!device_is_ready(dev)) {
		LOG_ERR("Auxdisplay device is not ready.");
		return;
	}

	rc = auxdisplay_cursor_set_enabled(dev, true);

	if (rc != 0) {
		LOG_ERR("Failed to enable cursor: %d", rc);
	}

	snprintk(data, sizeof(data), "Hello world from %s", CONFIG_BOARD);
	rc = auxdisplay_write(dev, data, strlen(data));

	if (rc != 0) {
		LOG_ERR("Failed to write data: %d", rc);
	}

#ifdef CONFIG_PM_DEVICE
	k_sleep(K_SECONDS(3));

	snprintk(data, sizeof(data), "Suspending display in 3 seconds...");
	rc = auxdisplay_clear(dev);
	if (rc != 0) {
		LOG_ERR("Failed to clear: %d", rc);
	}

	rc = auxdisplay_write(dev, data, strlen(data));
	if (rc != 0) {
		LOG_ERR("Failed to write data: %d", rc);
	}

	k_sleep(K_SECONDS(3));

	pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND);

	k_sleep(K_SECONDS(3));

	pm_device_action_run(dev, PM_DEVICE_ACTION_RESUME);

	snprintk(data, sizeof(data), "Display back online");
	rc = auxdisplay_clear(dev);

	if (rc != 0) {
		LOG_ERR("Failed to clear: %d", rc);
	}

	rc = auxdisplay_write(dev, data, strlen(data));

	if (rc != 0) {
		LOG_ERR("Failed to write data: %d", rc);
	}
#endif
}
