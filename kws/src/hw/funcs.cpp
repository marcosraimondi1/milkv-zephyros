#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/irq.h>
#include "adc.h"
#include "pin.h"
#include "timer.h"
#include "funcs.h"

// registers
#define ADC_PIN_MUX_OFFSET 0x0F8
#define ADC_PIN_FUNC       3U

// devicetree
#define PIN_MUX_BASE       (uint32_t)DT_REG_ADDR(DT_NODELABEL(pinctrl))
#define ADC_BASE           (uint32_t)DT_REG_ADDR(DT_ALIAS(myadc))
#define TIMER_BASE         (uint32_t)DT_REG_ADDR(DT_ALIAS(mytimer))
#define TIMER_IRQN         DT_IRQN(DT_ALIAS(mytimer))
#define TIMER_IRQ_PRIORITY DT_IRQ(DT_ALIAS(mytimer), priority)
#define LED0_NODE          DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

// constants
#define TIMER_ID          5
#define TIMER_CLK_FREQ_HZ 25000000
#define SAMPLING_FREQ_HZ  16000
#define TIMER_COUNT       (TIMER_CLK_FREQ_HZ / SAMPLING_FREQ_HZ) - 1

// variables
static int count = 0;
float *sample_buffer = NULL;

static void timer_stop()
{
	// disable timer
	hal_timer_disable_clk(TIMER_ID);
	TIMER_CTRL(TIMER_BASE) &= ~1;
}

static void adc_stop()
{
	hal_adc_reset_sel_channel(ADC_BASE, ADC_CHANNEL_SEL_Msk);
	hal_adc_stop(ADC_BASE);
	hal_adc_disable_clk();
}

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
			if (sample_buffer != NULL) {
				sample_buffer[count] = hal_adc_get_channel1_data(ADC_BASE) & 0xFFF;
			} else {
				printk("sample_buffer is NULL\n");
				while (1) {
				}
			}

			if (hal_adc_get_channel1_data_valid(ADC_BASE) == 0) {
				printk("ADC data is invalid\n");
				while (1) {
				}
			}

			count += 1;
			if (count >= NSAMPLES) {
				timer_stop();
				adc_stop();
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

static void irq_config(void)
{
	// Conectar la interrupcion (agrega a la tabla de interrupciones)
	IRQ_CONNECT(TIMER_IRQN, TIMER_IRQ_PRIORITY, timer_isr, NULL, 0);
	irq_enable(TIMER_IRQN);
}

static void timer_init(uint32_t load_count)
{
	// peripheral clock is 25MHz, 1s = 25M counts
	// disable timer
	hal_timer_disable_clk(TIMER_ID);
	TIMER_CTRL(TIMER_BASE) &= ~1;
	volatile int32_t a = TIMER_INTR_CLR(TIMER_BASE);
	ARG_UNUSED(a);

	// config timer
	TIMER_LOAD_COUNT(TIMER_BASE) = load_count;
	TIMER_CTRL(TIMER_BASE) &= ~(1 << 2); // interrupt not masked
	TIMER_CTRL(TIMER_BASE) |= 1 << 1;    // user-defined mode

	// enable timer
	hal_timer_enable_clk(TIMER_ID);
	TIMER_CTRL(TIMER_BASE) |= 1;
}

static void adc_init()
{
	uint8_t ch_id = 1;
	pin_set_mux(PIN_MUX_BASE + ADC_PIN_MUX_OFFSET, ADC_PIN_FUNC);
	hal_adc_enable_clk();
	hal_adc_set_sel_channel(ADC_BASE, (uint32_t)1U << (ADC_CTRL_ADC_SEL_Pos + ch_id));
	hal_adc_cyc_setting(ADC_BASE);
}

void get_audio(float *buffer)
{
	sample_buffer = buffer;
	gpio_pin_set_dt(&led, 1);
	adc_init();
	timer_init(TIMER_COUNT);
	irq_config();
	while (count < NSAMPLES) {
		k_sleep(K_MSEC(100));
	}
	count = 0;
	gpio_pin_set_dt(&led, 0);
}

void led_countdown(int countdown_s)
{
	gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	for (int i = 0; i < countdown_s; i++) {
		gpio_pin_set_dt(&led, 1);
		k_sleep(K_MSEC(500));
		gpio_pin_set_dt(&led, 0);
		k_sleep(K_MSEC(500));
	}
}
