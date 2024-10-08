#ifndef ENCODING_H
#define ENCODING_H

#include <pb_encode.h>
#include <pb_decode.h>
#include "kws/kws.h"

#define ERROR   -1
#define SUCCESS 0

int encode_msg(struct result_t result, uint8_t *buffer, size_t buffer_size);
int decode_msg(struct result_t *result, uint8_t *buffer, size_t message_length);

#endif // !ENCODING_H
