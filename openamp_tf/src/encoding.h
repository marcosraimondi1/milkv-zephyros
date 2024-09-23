#ifndef ENCODING_H
#define ENCODING_H

#include <pb_encode.h>
#include <pb_decode.h>

#define ERROR   -1
#define SUCCESS 0

int encode_board(char board[3][3], uint8_t *buffer, size_t buffer_size);
int decode_board(char board[3][3], uint8_t *buffer, size_t buffer_size);

#endif // !ENCODING_H
