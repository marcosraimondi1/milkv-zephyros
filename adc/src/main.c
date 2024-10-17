/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include "adc.h"
#include "zephyr/sys/printk.h"
#include <zephyr/timing/timing.h>

#define LED_GPIO_BASE    0x05021000
#define GPIO_SWPORTA_DDR (LED_GPIO_BASE + 0x004)
#define GPIO_SWPORTA_DR  (LED_GPIO_BASE + 0x000)
#define LED_PIN          2

#define ADC_BASE 0x030F0000

#define SLEEP_TIME_MS 500

int main(void)
{
	printk("ADC Example Starting\n");

	hal_adc_set_sel_channel(ADC_BASE, 0x10);
	printk("Selected channels %x\n", hal_adc_get_sel_channel(ADC_BASE));
	hal_adc_enable_clk();
	timing_init();
	timing_start();
	while (1) {
		timing_t start = timing_counter_get();
		hal_adc_start(ADC_BASE);
		while (!hal_adc_get_data_ready(ADC_BASE)) {
			printk("Waiting for data\n");
		}
		timing_t end = timing_counter_get();
		uint64_t cycles = timing_cycles_get(&start, &end);
		uint64_t ns = timing_cycles_to_ns(cycles);
		printk("ADC_CH1_RESULT %d\n", hal_adc_get_channel1_data(ADC_BASE));
		printk("ADC_CH1_VALID %d\n", hal_adc_get_channel1_data_valid(ADC_BASE));
		printk("TIME %lld\n", ns);
		k_msleep(SLEEP_TIME_MS);
	}
	return 0;
}
