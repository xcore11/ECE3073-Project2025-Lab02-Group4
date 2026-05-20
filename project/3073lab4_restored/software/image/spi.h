#ifndef SPI_H
#define SPI_H

#include <stdint.h>

void spi_init_manual(void);
void spi_request_text_from_esp(void);
void spi_send_session_start(void);
const char *spi_get_latest_text(void);
uint32_t spi_get_latest_packet_length(void);

#endif
