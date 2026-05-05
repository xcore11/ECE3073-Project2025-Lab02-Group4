#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <stdint.h>

/* =========================
   SDRAM memory map
   ========================= */

#ifndef TEXT_BUFFER_BASE
#define TEXT_BUFFER_BASE       0x05200000
#endif

#ifndef TEXT_BUFFER_SIZE
#define TEXT_BUFFER_SIZE       4096
#endif

#ifndef STATUS_BASE
#define STATUS_BASE            0x05201000
#endif

#ifndef SPI_PACKET_BASE
#define SPI_PACKET_BASE        0x05202000
#endif

#ifndef SPI_PACKET_MAX_SIZE
#define SPI_PACKET_MAX_SIZE    32768
#endif

/* =========================
   Status register offsets
   ========================= */

#define STATUS_TEXT_READY       0x00
#define STATUS_CMD_READY        0x04
#define STATUS_CMD_ACK          0x08
#define STATUS_SPI_RX_COUNT     0x0C
#define STATUS_LAST_ERROR       0x10
#define STATUS_PACKET_READY     0x14
#define STATUS_PACKET_LENGTH    0x18
#define STATUS_IMAGE_LENGTH     0x1C
#define STATUS_TEXT_LENGTH      0x20
#define STATUS_LINE_COUNT       0x24
#define STATUS_IMAGE_WIDTH      0x28
#define STATUS_IMAGE_HEIGHT     0x2C

#endif
