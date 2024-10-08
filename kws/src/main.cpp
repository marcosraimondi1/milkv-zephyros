#include <zephyr/kernel.h>
#include "zephyr/sys/printk.h"
#include "kws/kws.h"
#include "encoding/encoding.h"

int main()
{
	struct result_t result;
	struct result_t result_decoded;
	uint8_t buffer[128];
	while (1) {
		infer(&result);
		int message_size = encode_msg(result, buffer, sizeof(buffer));
		if (message_size == ERROR) {
			printk("Error encoding message\n");
			return 1;
		}

		printk("Encoded message size: %d\n", message_size);

		int decode_status = decode_msg(&result_decoded, buffer, message_size);
		if (decode_status == ERROR) {
			printk("Error decoding message\n");
			return 1;
		}

		printk("Predictions (DSP: %d ms., Classification: %d ms.): \n",
		       result_decoded.timing.dsp, result_decoded.timing.classification);

		for (size_t ix = 0; ix < NUM_CATEOGRIES; ix++) {
			printk("    %s: %f\n", result_decoded.predictions[ix].label,
			       (double)result_decoded.predictions[ix].value);
		}
	}
}
