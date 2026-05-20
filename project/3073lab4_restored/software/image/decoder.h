#ifndef DECODER_H
#define DECODER_H

#define DECODER_OK_SENT              1
#define DECODER_OK_WAITING_PORTAL    2
#define DECODER_NO_COMMAND           0
#define DECODER_ERR_BAD_FORMAT      -1
#define DECODER_ERR_OUT_OF_RANGE    -2
#define DECODER_ERR_MAILBOX_BUSY    -3

int decoder_decode_and_store_snake_command(const char *text);
int decoder_decode_and_store_snake_command_len(const char *text, unsigned int len);
void decoder_reset_stream(void);

#endif
