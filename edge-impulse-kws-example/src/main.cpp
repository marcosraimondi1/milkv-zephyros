// Zpehyr 3.1.x and newer uses different include scheme
#include <zephyr/kernel.h>
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include <zephyr/drivers/gpio.h>
#include "adc.h"
#include "pin.h"
#include "timer.h"

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
#define NSAMPLES          16000
#define TIMER_CLK_FREQ_HZ 25000000
#define SAMPLING_FREQ_HZ  16000
#define TIMER_COUNT       (TIMER_CLK_FREQ_HZ / SAMPLING_FREQ_HZ) - 1

// variables
static K_SEM_DEFINE(samples_ready, 0, 1);
static float features[NSAMPLES] = {0};
static int count = 0;

void led_countdown(int countdown_s);
int raw_feature_get_data(size_t offset, size_t length, float *out_ptr);
static void infer();
void get_audio();

int main()
{
	printk("Edge Impulse standalone inferencing (Zephyr)\n");
	setvbuf(stdout, NULL, _IONBF, 0);
	gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

	if (sizeof(features) / sizeof(float) != EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
		printk("The size of your 'features' array is not correct. Expected %d items, but "
		       "had %lu\n",
		       EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, sizeof(features) / sizeof(float));
		return 1;
	}

	while (1) {
		led_countdown(3);
		get_audio();
		infer();
	}
}

void timer_stop()
{
	// disable timer
	hal_timer_disable_clk(TIMER_ID);
	TIMER_CTRL(TIMER_BASE) &= ~1;
}

void adc_stop()
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
			features[count] = hal_adc_get_channel1_data(ADC_BASE) & 0xFFF;

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

void irq_config(void)
{
	// Conectar la interrupcion (agrega a la tabla de interrupciones)
	IRQ_CONNECT(TIMER_IRQN, TIMER_IRQ_PRIORITY, timer_isr, NULL, 0);
	irq_enable(TIMER_IRQN);
}

void timer_init(uint32_t load_count)
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

void adc_init()
{
	uint8_t ch_id = 1;
	pin_set_mux(PIN_MUX_BASE + ADC_PIN_MUX_OFFSET, ADC_PIN_FUNC);
	hal_adc_enable_clk();
	hal_adc_set_sel_channel(ADC_BASE, (uint32_t)1U << (ADC_CTRL_ADC_SEL_Pos + ch_id));
	hal_adc_cyc_setting(ADC_BASE);
}

static void infer()
{
	ei_impulse_result_t result = {0};
	signal_t features_signal;
	features_signal.total_length = sizeof(features) / sizeof(features[0]);
	features_signal.get_data = &raw_feature_get_data;

	// invoke the impulse
	printk("Running the impulse...\n");
	EI_IMPULSE_ERROR res = run_classifier(&features_signal, &result, false);

	if (res != 0) {
		printk("ERR: Failed to run classifier (%d)\n", res);
		return;
	}

	printk("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
	       result.timing.dsp, result.timing.classification, result.timing.anomaly);

	for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
		printk("    %s: %f\n", result.classification[ix].label,
		       (double)result.classification[ix].value);
	}
}
void get_audio()
{
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

int raw_feature_get_data(size_t offset, size_t length, float *out_ptr)
{
	memcpy(out_ptr, features + offset, length * sizeof(float));
	return 0;
}

void led_countdown(int countdown_s)
{
	for (int i = 0; i < countdown_s; i++) {
		gpio_pin_set_dt(&led, 1);
		k_sleep(K_MSEC(500));
		gpio_pin_set_dt(&led, 0);
		k_sleep(K_MSEC(500));
	}
}
