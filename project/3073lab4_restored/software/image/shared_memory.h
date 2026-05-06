#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <stdint.h>

/* =========================
   SDRAM memory map

   Image processor writes here.
   VGA processor only reads these same addresses.
   ========================= */

#ifndef TEXT_BUFFER_BASE
#define TEXT_BUFFER_BASE       0x05200000
#endif

#ifndef TEXT_BUFFER_SIZE
#define TEXT_BUFFER_SIZE       4096
#endif

/*
   Internal 32-bit status registers for the image/SPI processor.
   These are NOT the 24-byte ESP packet header.
*/
#ifndef STATUS_REG_BASE
#define STATUS_REG_BASE        0x05201000
#endif

/*
   Full raw ESP packet storage:
   [24-byte header][text bytes][96x96 raw grayscale bytes]
*/
#ifndef SPI_PACKET_BASE
#define SPI_PACKET_BASE        0x05202000
#endif

/*
   Keep full packet below STATUS_BASE at 0x05209000.
   0x05209000 - 0x05202000 = 0x7000 = 28672 bytes.
   Your real packet is around 9.2 KB, so this is enough.
*/
#ifndef SPI_PACKET_MAX_SIZE
#define SPI_PACKET_MAX_SIZE    0x7000
#endif

/*
   24-byte packet header copy for VGA/status checking.
   VGA checks STATUS_BASE[0..23] and expects magic 'G''V'.
*/
#ifndef STATUS_BASE
#define STATUS_BASE            0x05209000
#endif

#ifndef STATUS_HEADER_SIZE
#define STATUS_HEADER_SIZE     24
#endif

/*
   Raw 96x96 grayscale image buffer for VGA display.
*/
#ifndef IMAGE_BUFFER_BASE
#define IMAGE_BUFFER_BASE      0x0520A000
#endif

#ifndef IMAGE_BUFFER_SIZE
#define IMAGE_BUFFER_SIZE      32768
#endif

#ifndef IMAGE_WIDTH_DEFAULT
#define IMAGE_WIDTH_DEFAULT    96
#endif

#ifndef IMAGE_HEIGHT_DEFAULT
#define IMAGE_HEIGHT_DEFAULT   96
#endif

#ifndef IMAGE_LENGTH_DEFAULT
#define IMAGE_LENGTH_DEFAULT   (IMAGE_WIDTH_DEFAULT * IMAGE_HEIGHT_DEFAULT)
#endif

/* =========================
   Internal 32-bit status register offsets
   Base = STATUS_REG_BASE
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

/* =========================
   ESP packet header byte offsets
   Base = STATUS_BASE when copied for VGA
   ========================= */

#define PKT_MAGIC0_OFS          0
#define PKT_MAGIC1_OFS          1
#define PKT_VERSION_OFS         2
#define PKT_STATUS_OFS          3
#define PKT_TOTAL_LEN_OFS       4
#define PKT_TEXT_LEN_OFS        8
#define PKT_IMAGE_W_OFS         10
#define PKT_IMAGE_H_OFS         12
#define PKT_LINE_COUNT_OFS      14
#define PKT_IMAGE_LEN_OFS       16
#define PKT_HEADER_LEN_OFS      20
#define PKT_FLAGS_OFS           22

#endif
