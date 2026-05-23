#ifndef SPI_H
#define SPI_H

#include <stdint.h>

void spi_init_manual(void);
void spi_send_session_start(void);
void spi_send_panel_mode(uint32_t panel_mode);
void spi_send_debug_image_capture(void);
void spi_request_text_from_esp(void);
const char *spi_get_latest_text(void);
uint32_t spi_get_latest_packet_length(void);

#endif
