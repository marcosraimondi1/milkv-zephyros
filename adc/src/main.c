/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>

#define LED_GPIO_BASE    0x05021000
#define GPIO_SWPORTA_DDR (LED_GPIO_BASE + 0x004)
#define GPIO_SWPORTA_DR  (LED_GPIO_BASE + 0x000)
#define LED_PIN          2

#define SLEEP_TIME_MS 100

int main(void)
{
	// configure LED GPIO PIN
	// Step 1: Configure the register GPIO_SWPORTA_DDR, set whether the GPIO is used as input or
	// output.
	uint32_t *gpio_swporta_ddr = (uint32_t *)GPIO_SWPORTA_DDR;
	*gpio_swporta_ddr |= 1 << LED_PIN; // set LED_PIN as output

	while (1) {
		// toggle LED GPIO PIN
		// write the output value to the GPIO_SWPORTA_DR register to control the
		// GPIO output level.
		printk("Toggle led\n");
		uint32_t *gpio_swporta_dr = (uint32_t *)GPIO_SWPORTA_DR;
		*gpio_swporta_dr ^= 1 << LED_PIN; // toggle LED_PIN

		k_msleep(SLEEP_TIME_MS);
	}
	return 0;
}
