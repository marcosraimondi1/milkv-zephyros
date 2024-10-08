#include <zephyr/kernel.h>
#include "kws/kws.h"
#include "zephyr/sys/printk.h"

int main()
{
	struct result_t result;
	while (1) {
		infer(&result);

		printk("Predictions (DSP: %d ms., Classification: %d ms.): \n", result.timing.dsp,
		       result.timing.classification);

		for (size_t ix = 0; ix < NUM_CATEOGRIES; ix++) {
			printk("    %s: %f\n", result.predictions[ix].label,
			       (double)result.predictions[ix].value);
		}
	}
}
