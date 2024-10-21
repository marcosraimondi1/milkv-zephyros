/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include "adc.h"
#include "pin.h"
#include "zephyr/sys/printk.h"
#include <zephyr/timing/timing.h>

#define LED_GPIO_BASE    0x05021000
#define GPIO_SWPORTA_DDR (LED_GPIO_BASE + 0x004)
#define GPIO_SWPORTA_DR  (LED_GPIO_BASE + 0x000)
#define LED_PIN          2

#define ADC_BASE           0x030F0000UL
#define PIN_MUX_BASE       0x03001000UL
#define ADC_PIN_MUX_OFFSET 0x0F8
#define ADC_PIN_FUNC       3U

#define SLEEP_TIME_MS 500

int16_t analogRead()
{
	int32_t adc_data = 0;
	uint8_t ch_id = 1;
	pin_set_mux(PIN_MUX_BASE + ADC_PIN_MUX_OFFSET, ADC_PIN_FUNC);
	hal_adc_enable_clk();
	hal_adc_set_sel_channel(ADC_BASE, (uint32_t)1U << (ADC_CTRL_ADC_SEL_Pos + ch_id));
	hal_adc_cyc_setting(ADC_BASE);
	hal_adc_start(ADC_BASE);

	while (hal_adc_get_data_ready(ADC_BASE) == 0) {
		k_sleep(K_USEC(100));
	}

	adc_data = hal_adc_get_channel1_data(ADC_BASE);

	if (hal_adc_get_channel1_data_valid(ADC_BASE) == 0) {
		printk("ADC data is invalid\n");
		adc_data = -1;
	} else {
		adc_data = adc_data & 0xFFF;
	}

	hal_adc_reset_sel_channel(ADC_BASE, ADC_CHANNEL_SEL_Msk);
	hal_adc_stop(ADC_BASE);
	hal_adc_disable_clk();

	return adc_data;
}

int main(void)
{
	printk("ADC Example Starting\n");

	while (1) {
		timing_t start = timing_counter_get();

		int16_t adc_data = analogRead();

		timing_t end = timing_counter_get();
		uint64_t cycles = timing_cycles_get(&start, &end);
		uint64_t ns = timing_cycles_to_ns(cycles);

		printk("ADC %d\n", adc_data);
		printk("TIME %lld [ms]\n", ns / 1000);
		k_msleep(SLEEP_TIME_MS);
	}
	return 0;
}
