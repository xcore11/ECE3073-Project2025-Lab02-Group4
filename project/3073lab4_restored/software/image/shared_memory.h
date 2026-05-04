#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <stdint.h>

/* =========================
   SDRAM memory map
   ========================= */

#define TEXT_BUFFER_BASE        0x05200000
#define TEXT_BUFFER_SIZE        256

#define STATUS_BASE             0x05201000
#define STATUS_SIZE             0x00001000

#define INSTRUCTION_BASE        0x05203000
#define INSTRUCTION_SIZE        0x00001000

/* =========================
   STATUS register offsets
   ========================= */

#define STATUS_TEXT_READY       0x00
#define STATUS_CMD_READY        0x04
#define STATUS_CMD_ACK          0x08
#define STATUS_SPI_RX_COUNT     0x0C
#define STATUS_LAST_ERROR       0x10

/* =========================
   General instruction offsets
   ========================= */

#define CMD_VALID               0x00
#define CMD_TARGET_MASK         0x04
#define CMD_SEQUENCE_ID         0x08
#define CMD_ERROR_FLAGS         0x0C

/* =========================
   HEX display registers
   ========================= */

#define HEX_ENABLE              0x10
#define HEX_MODE                0x14
#define HEX_MSG_LEN             0x18
#define HEX_MSG_BASE            0x20
#define HEX_MSG_MAX_LEN         128

/* =========================
   LED Module registers
   ========================= */

#define LEDMOD_ENABLE           0xA0
#define LEDMOD_COLOR_MASK       0xA4
#define LEDMOD_MODE             0xA8
#define LEDMOD_PERIOD_MS        0xAC

/* =========================
   FPGA LED registers
   ========================= */

#define FPGALED_ENABLE          0xC0
#define FPGALED_COUNT           0xC4
#define FPGALED_DIRECTION       0xC8
#define FPGALED_MODE            0xCC
#define FPGALED_PERIOD_MS       0xD0

/* =========================
   Speaker registers
   ========================= */

#define SPEAKER_ENABLE          0xE0
#define SPEAKER_FREQ_HZ         0xE4
#define SPEAKER_DURATION_MS     0xE8

/* =========================
   Target mask bits
   ========================= */

#define TARGET_NONE             0x00
#define TARGET_HEX              0x01
#define TARGET_LED_MODULE       0x02
#define TARGET_FPGA_LED         0x04
#define TARGET_SPEAKER          0x08

/* =========================
   Common modes
   ========================= */

#define MODE_STATIC             0
#define MODE_BLINK              1
#define MODE_SCROLL             2

/* =========================
   LED module color bits
   ========================= */

#define LEDMOD_COLOR_NONE       0x00
#define LEDMOD_COLOR_RED        0x01
#define LEDMOD_COLOR_GREEN      0x02
#define LEDMOD_COLOR_YELLOW     0x04

/* =========================
   FPGA LED direction
   ========================= */

#define DIR_LEFT                0
#define DIR_RIGHT               1

/* =========================
   Error flags
   ========================= */

#define ERR_NONE                0x00
#define ERR_UNKNOWN_PERIPHERAL  0x01
#define ERR_MISSING_COLOR       0x02
#define ERR_MISSING_PERIOD      0x04
#define ERR_MISSING_LED_COUNT   0x08
#define ERR_MISSING_DIRECTION   0x10
#define ERR_MISSING_FREQUENCY   0x20
#define ERR_MISSING_HEX_MSG     0x40

#endif
