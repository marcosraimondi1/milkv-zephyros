#include "encoding.h"
#include "src/encoding/message.pb.h"
#include <zephyr/kernel.h>

int encode_msg(struct result_t result, uint8_t *buffer, size_t buffer_size)
{
	KWS_Result message = KWS_Result_init_zero;

	for (int i = 0; i < 3; i++) {
		message.predictions[i].value = result.predictions[i].value;
		strncpy(message.predictions[i].label, result.predictions[i].label, 32);
	}
	message.predictions_count = 3;

	message.timing.dsp = result.timing.dsp;
	message.timing.classification = result.timing.classification;

	// create stream that will write to our buffer
	pb_ostream_t stream = pb_ostream_from_buffer(buffer, buffer_size);

	// Encode the message
	bool status = pb_encode(&stream, KWS_Result_fields, &message);
	size_t message_length = stream.bytes_written;

	if (!status) {
		printk("Encoding failed: %s\n", PB_GET_ERROR(&stream));
		return ERROR;
	}

	return message_length;
}

int decode_msg(struct result_t *result, uint8_t *buffer, size_t message_length)
{
	KWS_Result message = KWS_Result_init_zero;

	// Create a stream that reads from the buffer
	pb_istream_t stream = pb_istream_from_buffer(buffer, message_length);

	// Decode the message
	int status = pb_decode(&stream, KWS_Result_fields, &message);

	// Check for decoding errors
	if (!status) {
		printk("Decoding failed: %s\n", PB_GET_ERROR(&stream));
		return ERROR;
	}

	for (int i = 0; i < 3; i++) {
		result->predictions[i].value = message.predictions[i].value;
		strncpy(result->predictions[i].label, message.predictions[i].label, 32);
	}

	result->timing.dsp = message.timing.dsp;
	result->timing.classification = message.timing.classification;

	return SUCCESS;
}
