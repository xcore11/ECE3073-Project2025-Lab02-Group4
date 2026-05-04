#ifndef SPI_H
#define SPI_H

void spi_init_manual(void);
void spi_request_text_from_esp(void);
const char *spi_get_latest_text(void);

#endif
