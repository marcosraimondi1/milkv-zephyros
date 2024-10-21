/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include "adc.h"
#include "pin.h"
#include "timer.h"
#include "zephyr/sys/printk.h"
#include <zephyr/timing/timing.h>

// registers
#define ADC_BASE           0x030F0000UL
#define PIN_MUX_BASE       0x03001000UL
#define ADC_PIN_MUX_OFFSET 0x0F8
#define ADC_PIN_FUNC       3U
#define TIMER4_BASE        0x030A003CUL

// constants
#define SLEEP_TIME_MS 2
#define NSAMPLES      16000

// variables
static int16_t samples[NSAMPLES] = {0};

// functions
void analogReadValues();
int16_t analogRead();
void timer_init(uint32_t load_count);

int main(void)
{
	printk("ADC Example Starting\n");
	timer_init(250000000);
	while (1) {
		// analogReadValues();
		//
		// for (int i = 0; i < NSAMPLES; i++) {
		// 	printk("%d \n", samples[i]);
		// }

		printk("Timer current value %d\n", TIMER_CURRENT_VALUE(TIMER4_BASE));
		k_sleep(K_MSEC(1000));
	}
	return 0;
}

void timer_init(uint32_t load_count)
{
	// disable timer
	hal_timer4_disable_clk();
	TIMER_CTRL(TIMER4_BASE) &= ~1;

	// config timer
	TIMER_LOAD_COUNT(TIMER4_BASE) = load_count;
	TIMER_CTRL(TIMER4_BASE) &= ~(1 << 2); // interrupt not masked
	TIMER_CTRL(TIMER4_BASE) |= 1 << 1;    // user-defined mode

	// enable timer
	hal_timer4_enable_clk();
	TIMER_CTRL(TIMER4_BASE) |= 1;
}

void adc_init()
{
	uint8_t ch_id = 1;
	pin_set_mux(PIN_MUX_BASE + ADC_PIN_MUX_OFFSET, ADC_PIN_FUNC);
	hal_adc_enable_clk();
	hal_adc_set_sel_channel(ADC_BASE, (uint32_t)1U << (ADC_CTRL_ADC_SEL_Pos + ch_id));
	hal_adc_cyc_setting(ADC_BASE);
}

int16_t adc_read()
{
	int16_t sample;

	hal_adc_start(ADC_BASE); // start conversion
	while (hal_adc_get_data_ready(ADC_BASE) == 0) {
		// wait value
		k_sleep(K_USEC(1));
	}

	sample = hal_adc_get_channel1_data(ADC_BASE) & 0xFFF;

	if (hal_adc_get_channel1_data_valid(ADC_BASE) == 0) {
		printk("ADC data is invalid\n");
		return -1;
	}

	return sample;
}

void adc_stop()
{
	hal_adc_reset_sel_channel(ADC_BASE, ADC_CHANNEL_SEL_Msk);
	hal_adc_stop(ADC_BASE);
	hal_adc_disable_clk();
}

void analogReadValues()
{
	adc_init();

	for (int i = 0; i < NSAMPLES; i++) {
		samples[i] = adc_read();
	}

	adc_stop();

	return;
}

int16_t analogRead()
{
	int16_t adc_data = 0;
	adc_init();
	adc_data = adc_read();
	adc_stop();
	return adc_data;
}
