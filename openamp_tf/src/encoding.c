#include "encoding.h"
#include "src/message.pb.h"
#include <zephyr/kernel.h>

Board_Mark get_mark(char mark)
{
	switch (mark) {
	case 'X':
		return Board_Mark_MARK_X;
	case 'O':
		return Board_Mark_MARK_O;
	default:
		return Board_Mark_MARK_EMPTY;
	}
}

char get_mark_char(Board_Mark mark)
{
	switch (mark) {
	case Board_Mark_MARK_X:
		return 'X';
	case Board_Mark_MARK_O:
		return 'O';
	default:
		return ' ';
	}
}

int encode_board(char board[3][3], uint8_t *buffer, size_t buffer_size)
{
	Board message = Board_init_zero;

	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			message.rows[i].marks[j] = get_mark(board[i][j]);
		}
		message.rows[i].marks_count = 3;
	}
	message.rows_count = 3;

	// create stream that will write to our buffer
	pb_ostream_t stream = pb_ostream_from_buffer(buffer, buffer_size);

	// Encode the message
	bool status = pb_encode(&stream, Board_fields, &message);
	size_t message_length = stream.bytes_written;

	if (!status) {
		printk("Encoding failed: %s\n", PB_GET_ERROR(&stream));
		return ERROR;
	}

	return message_length;
}

int decode_board(char board[3][3], uint8_t *buffer, size_t message_length)
{
	/* Initialize the message structure to zero */
	Board message = Board_init_zero;

	/* Create a stream that reads from the buffer */
	pb_istream_t stream = pb_istream_from_buffer(buffer, message_length);

	/* Decode the message */
	int status = pb_decode(&stream, Board_fields, &message);

	/* Check for decoding errors */
	if (!status) {
		printk("Decoding failed: %s\n", PB_GET_ERROR(&stream));
		return ERROR;
	}

	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			board[i][j] = get_mark_char(message.rows[i].marks[j]);
		}
	}

	return SUCCESS;
}
