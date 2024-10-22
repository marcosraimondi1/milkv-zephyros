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
#include <zephyr/irq.h>

// registers
#define ADC_BASE           0x030F0000UL
#define PIN_MUX_BASE       0x03001000UL
#define ADC_PIN_MUX_OFFSET 0x0F8
#define ADC_PIN_FUNC       3U

// constants
#define NSAMPLES          16000
#define TIMER_CLK_FREQ_HZ 25000000
#define SAMPLING_FREQ_HZ  16000
#define TIMER_COUNT       (TIMER_CLK_FREQ_HZ / SAMPLING_FREQ_HZ) - 1

// functions
void analogReadValues();
int16_t analogRead();
void adc_init();
int16_t adc_read();
void adc_stop();
void timer_init(uint32_t load_count);
void timer_stop();

// variables
static int16_t count = 0;
static int16_t values[NSAMPLES] = {0};

// ISR
static void timer_isr()
{
	static bool first = true;
	if (TIMER_INTR_STA(TIMER_BASE) & 1) {
		volatile int32_t a = TIMER_INTR_CLR(TIMER_BASE);
		ARG_UNUSED(a); // clear interrupt

		if (first) {
			first = false;
			hal_adc_start(ADC_BASE); // start conversion
			return;
		}

		if (hal_adc_get_data_ready(ADC_BASE)) {
			values[count] = hal_adc_get_channel1_data(ADC_BASE) & 0xFFF;

			if (hal_adc_get_channel1_data_valid(ADC_BASE) == 0) {
				printk("ADC data is invalid\n");
				while (1) {
				}
			}

			count += 1;
			if (count >= NSAMPLES) {
				timer_stop();
				adc_stop();
				// for (int i = 0; i < NSAMPLES; i++) {
				// 	printk("%d\n", values[i]);
				// }
				printk("ADC Example Finished\n");
				return;
			}
			hal_adc_start(ADC_BASE); // start conversion
		} else {
			printk("ADC data is not ready\n");
			while (1) {
			}
		}
	}
	return;
}

void irq_config(void)
{
	// Obtener el numero de interrupcion directamente desde el devicetree
	const int irq_num = DT_IRQN(DT_ALIAS(mytimer));
	printk("IRQ number: %d\n", irq_num);

	// Obtener la prioridad de la interrupcion
	const int irq_priority = DT_IRQ(DT_ALIAS(mytimer), priority);
	printk("IRQ priority: %d\n", irq_priority);

	// Conectar la interrupcion (agrega a la tabla de interrupciones)
	IRQ_CONNECT(irq_num, irq_priority, timer_isr, NULL, 0);

	// Habilitar la interrupcion
	irq_enable(irq_num);
}

int main(void)
{
	printk("ADC Example Starting\n");
	adc_init();
	timer_init(TIMER_COUNT);
	irq_config();

	while (1) {
		k_sleep(K_MSEC(100));
	}
	return 0;
}

void timer_stop()
{
	// disable timer
	hal_timer_disable_clk();
	TIMER_CTRL(TIMER_BASE) &= ~1;
}

void timer_init(uint32_t load_count)
{
	// peripheral clock is 25MHz, 1s = 25M counts
	// disable timer
	hal_timer_disable_clk();
	TIMER_CTRL(TIMER_BASE) &= ~1;
	volatile int32_t a = TIMER_INTR_CLR(TIMER_BASE);
	ARG_UNUSED(a);

	// config timer
	TIMER_LOAD_COUNT(TIMER_BASE) = load_count;
	TIMER_CTRL(TIMER_BASE) &= ~(1 << 2); // interrupt not masked
	TIMER_CTRL(TIMER_BASE) |= 1 << 1;    // user-defined mode

	// enable timer
	hal_timer_enable_clk();
	TIMER_CTRL(TIMER_BASE) |= 1;
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

int16_t analogRead()
{
	int16_t adc_data = 0;
	adc_init();
	adc_data = adc_read();
	adc_stop();
	return adc_data;
}
