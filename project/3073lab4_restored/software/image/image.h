#include <stdint.h>

#ifndef IMAGE_H
#define IMAGE_H

/* SPI */
void spi_init_manual(void);
void spi_start_capture(const char *message);
void spi_service(void);
int spi_is_busy(void);
int spi_has_valid_message(void);
void spi_get_message(char *buf, int max_len);

/* image / shared SDRAM */
#define FRAMEBUFFER0_BASE 0x05000000
#define FRAMEBUFFER1_BASE 0x05100000
#define TEXT_BUFFER_BASE  0x05200000
#define STATUS_BASE       0x05201000

#endif
