// Zpehyr 3.1.x and newer uses different include scheme
#include <zephyr/kernel.h>
#include "features.h"
#include "features1.h"
#include "features2.h"
#include "features3.h"
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "edge-impulse-sdk/dsp/numpy.hpp"

int raw_feature_get_data(size_t offset, size_t length, float *out_ptr)
{
	memcpy(out_ptr, features + offset, length * sizeof(float));
	return 0;
}

int raw_feature1_get_data(size_t offset, size_t length, float *out_ptr)
{
	memcpy(out_ptr, features1 + offset, length * sizeof(float));
	return 0;
}

int raw_feature2_get_data(size_t offset, size_t length, float *out_ptr)
{
	memcpy(out_ptr, features2 + offset, length * sizeof(float));
	return 0;
}

int raw_feature3_get_data(size_t offset, size_t length, float *out_ptr)
{
	memcpy(out_ptr, features3 + offset, length * sizeof(float));
	return 0;
}

int main()
{
	// This is needed so that output of printf is output immediately without buffering
	setvbuf(stdout, NULL, _IONBF, 0);

	printk("Edge Impulse standalone inferencing (Zephyr)\n");

	if (sizeof(features3) / sizeof(float) != EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
		printk("The size of your 'features' array is not correct. Expected %d items, but "
		       "had %lu\n",
		       EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, sizeof(features3) / sizeof(float));
		return 1;
	}

	ei_impulse_result_t result = {0};

	while (1) {
		// the features are stored into flash, and we don't want to load everything into RAM
		signal_t features_signal;
		features_signal.total_length = sizeof(features3) / sizeof(features3[0]);
		features_signal.get_data = &raw_feature3_get_data;

		// invoke the impulse
		EI_IMPULSE_ERROR res = run_classifier(&features_signal, &result, true);
		printk("run_classifier returned: %d\n", res);

		if (res != 0) {
			return 1;
		}

		printk("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
		       result.timing.dsp, result.timing.classification, result.timing.anomaly);

		for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
			printk("    %s: %f\n", result.classification[ix].label,
			       (double)result.classification[ix].value);
		}

		k_msleep(2000);
	}
}
