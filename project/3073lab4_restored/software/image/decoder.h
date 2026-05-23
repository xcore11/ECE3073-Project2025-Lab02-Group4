#ifndef DECODER_H
#define DECODER_H

#define DECODER_OK_SENT              1
#define DECODER_OK_WAITING_PORTAL    2
#define DECODER_OK_STATE_UPDATED     3
#define DECODER_OK_BATCH_SENT        4
#define DECODER_NO_COMMAND           0
#define DECODER_ERR_BAD_FORMAT      -1
#define DECODER_ERR_OUT_OF_RANGE    -2
#define DECODER_ERR_MAILBOX_BUSY    -3
#define DECODER_ERR_BAD_PANEL       -4

#define DECODER_PANEL_DEBUG          3
#define DECODER_PANEL_SNAKE          1
#define DECODER_PANEL_DRAW           2
#define DECODER_PANEL_BATTLE         4

int decoder_decode_and_store_panel_text(int panel_mode, const char *text);
int decoder_decode_and_store_panel_text_len(int panel_mode, const char *text, unsigned int len);
int decoder_decode_and_store_panel_text_batch(int panel_mode, const char *text, unsigned int len);

int decoder_decode_and_store_snake_command(const char *text);
int decoder_decode_and_store_snake_command_len(const char *text, unsigned int len);

void decoder_reset_stream(void);

#endif
