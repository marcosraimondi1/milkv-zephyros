// Zpehyr 3.1.x and newer uses different include scheme
#include "kws.h"
#include <zephyr/kernel.h>
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "yes_features.h"
#include "unknown_features.h"
#include "noise_features.h"

static int select_features = 0;
int raw_feature_get_data(size_t offset, size_t length, float *out_ptr)
{
	const float *features = NULL;
	switch (select_features) {
	case 1:
		features = yes_features;
		break;
	case 2:
		features = unknown_features;
		break;
	default:
		features = noise_features;
		select_features = 0;
		break;
	}
	memcpy(out_ptr, features + offset, length * sizeof(float));
	return 0;
}

int infer(struct result_t *res)
{
	ei_impulse_result_t result = {0};
	signal_t features_signal;
	features_signal.total_length = sizeof(yes_features) / sizeof(yes_features[0]);
	features_signal.get_data = &raw_feature_get_data;

	// invoke the impulse
	EI_IMPULSE_ERROR status = run_classifier(&features_signal, &result, false);
	if (status != 0) {
		return 1;
	}

	for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
		strncpy(res->predictions[ix].label, result.classification[ix].label,
			sizeof(res->predictions[ix].label) - 1);
		res->predictions[ix].value = result.classification[ix].value;
	}

	res->timing.dsp = result.timing.dsp;
	res->timing.classification = result.timing.classification;

	select_features += 1;
}
