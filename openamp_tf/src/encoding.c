#include "encoding.h"
#include "src/message.pb.h"
#include <zephyr/kernel.h>

int encode_board(char board[3][3], uint8_t *buffer, size_t buffer_size)
{
	int32_t board_data[9];
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			board_data[i * 3 + j] = (int32_t)board[i][j];
		}
	}

	SimpleMessage message = SimpleMessage_init_zero;
	memcpy(message.board, board_data, sizeof(board_data));
	message.board_count = 9; // Set the actual number of elements

	// create stream that will write to our buffer
	pb_ostream_t stream =
		pb_ostream_from_buffer(buffer + sizeof(size_t), buffer_size - sizeof(size_t));

	// Encode the message
	bool status = pb_encode(&stream, SimpleMessage_fields, &message);
	size_t message_length = stream.bytes_written;

	if (!status) {
		printk("Encoding failed: %s\n", PB_GET_ERROR(&stream));
		return ERROR;
	}

	// Write the message length to the beginning of the buffer
	memcpy(buffer, &message_length, sizeof(size_t));

	return message_length + sizeof(size_t);
}

int decode_board(char board[3][3], uint8_t *buffer)
{
	/* Initialize the message structure to zero */
	SimpleMessage message = SimpleMessage_init_zero;

	// Read the message length
	size_t message_length;
	memcpy(&message_length, buffer, sizeof(size_t));

	/* Create a stream that reads from the buffer */
	pb_istream_t stream = pb_istream_from_buffer(buffer + sizeof(size_t), message_length);

	/* Decode the message */
	int status = pb_decode(&stream, SimpleMessage_fields, &message);

	/* Check for decoding errors */
	if (!status) {
		printk("Decoding failed: %s\n", PB_GET_ERROR(&stream));
		return ERROR;
	}

	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			board[i][j] = (char)message.board[i * 3 + j];
		}
	}

	return SUCCESS;
}
