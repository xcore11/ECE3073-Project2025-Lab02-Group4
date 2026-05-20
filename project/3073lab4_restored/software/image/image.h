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
void spi_init_manual(void);
void spi_request_text_from_esp(void);
const char *spi_get_latest_text(void);

/* =========================
   SDRAM memory map
   ========================= */

#define FRAMEBUFFER0_BASE       0x05000000  // 32-bit address
#define FRAMEBUFFER1_BASE       0x05100000  // 32-bit address

#define TEXT_BUFFER_BASE        0x05200000  // 32-bit address
#define STATUS_BASE             0x05201000  // 32-bit address
#define INSTRUCTION_BASE        0x05202000  // 32-bit address

#endif
