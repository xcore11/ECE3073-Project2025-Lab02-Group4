#ifndef DECODERDEBUG_H
#define DECODERDEBUG_H

#include <stdint.h>

#define DEBUG_DECODER_NO_COMMAND       0
#define DEBUG_DECODER_PUBLISHED        1

void decoder_debug_reset(void);
int decoder_debug_should_hide_vga_text(const char *text, unsigned int len);
int decoder_debug_decode_text(const char *text, unsigned int len);

#endif
