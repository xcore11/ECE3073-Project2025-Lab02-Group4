#include "io.h"
#include "system.h"
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include "shared_memory.h"
#include "spi.h"

/* =========================
   SPI register base
   ========================= */

#define SPI_BASE            SPI_0_BASE

/* =========================
   SPI register offsets
   ========================= */

#define SPI_RXDATA_OFS      0
#define SPI_TXDATA_OFS      4
#define SPI_STATUS_OFS      8
#define SPI_CONTROL_OFS     12
#define SPI_SLAVESEL_OFS    20

/* =========================
   SPI status bits
   ========================= */

#define SPI_STATUS_RRDY     (1 << 7)
#define SPI_STATUS_TRDY     (1 << 6)

/* =========================
   SPI control bits
   ========================= */

#define SPI_CONTROL_SSO     (1 << 10)

/* =========================
   SPI protocol
   ========================= */

#define SPI_SLAVE_0         0x1
#define SPI_REQUEST_BYTE    0x5F
#define SPI_DUMMY_BYTE      0x00
#define SPI_START_BYTE      0xA5

#define SPI_MAX_TEXT_LEN    128

static char spi_rx_text[SPI_MAX_TEXT_LEN + 1];

/* =========================
   SDRAM helpers
   ========================= */

static void sdram_write_status(uint32_t offset, uint32_t value)
{
    IOWR_32DIRECT(STATUS_BASE, offset, value);
}

static void sdram_store_text(const char *text)
{
    volatile char *dst = (volatile char *)TEXT_BUFFER_BASE;
    int i;

    for (i = 0; i < TEXT_BUFFER_SIZE; i++) {
        dst[i] = '\0';
    }

    for (i = 0; i < TEXT_BUFFER_SIZE - 1 && text[i] != '\0'; i++) {
        dst[i] = text[i];
    }

    dst[i] = '\0';

    sdram_write_status(STATUS_SPI_RX_COUNT, (uint32_t)i);
    sdram_write_status(STATUS_TEXT_READY, 1);
}

/* =========================
   Basic SPI helpers
   ========================= */

void spi_init_manual(void)
{
    IOWR_32DIRECT(SPI_BASE, SPI_SLAVESEL_OFS, SPI_SLAVE_0);
    IOWR_32DIRECT(SPI_BASE, SPI_CONTROL_OFS, 0);

    sdram_write_status(STATUS_TEXT_READY, 0);
    sdram_write_status(STATUS_CMD_READY, 0);
    sdram_write_status(STATUS_CMD_ACK, 0);
    sdram_write_status(STATUS_SPI_RX_COUNT, 0);
    sdram_write_status(STATUS_LAST_ERROR, 0);
}

static void spi_begin_transaction(void)
{
    IOWR_32DIRECT(SPI_BASE, SPI_SLAVESEL_OFS, SPI_SLAVE_0);
    IOWR_32DIRECT(SPI_BASE, SPI_CONTROL_OFS, SPI_CONTROL_SSO);
}

static void spi_end_transaction(void)
{
    IOWR_32DIRECT(SPI_BASE, SPI_CONTROL_OFS, 0);
}

static uint8_t spi_transfer_byte(uint8_t tx_byte)
{
    uint8_t rx_byte;

    while (!(IORD_32DIRECT(SPI_BASE, SPI_STATUS_OFS) & SPI_STATUS_TRDY));

    IOWR_32DIRECT(SPI_BASE, SPI_TXDATA_OFS, tx_byte);

    while (!(IORD_32DIRECT(SPI_BASE, SPI_STATUS_OFS) & SPI_STATUS_RRDY));

    rx_byte = (uint8_t)(IORD_32DIRECT(SPI_BASE, SPI_RXDATA_OFS) & 0xFF);

    return rx_byte;
}

/* =========================
   Main SPI request function
   ========================= */

void spi_request_text_from_esp(void)
{
    uint8_t start_byte;
    uint8_t text_len;
    int i;

    for (i = 0; i < SPI_MAX_TEXT_LEN + 1; i++) {
        spi_rx_text[i] = '\0';
    }

    printf("SPI: sending request byte 0x%02X\n", SPI_REQUEST_BYTE);
    fflush(stdout);

    /*
       Transaction 1:
       FPGA sends request byte 0x5F to ESP.
    */
    spi_begin_transaction();
    spi_transfer_byte(SPI_REQUEST_BYTE);
    spi_end_transaction();

    /*
       Small delay so ESP can prepare the reply packet.
    */
    usleep(50000);

    printf("SPI: reading reply packet\n");
    fflush(stdout);

    /*
       Transaction 2:
       FPGA sends dummy bytes to clock back:
       [0] = start byte
       [1] = length
       [2..] = text
    */
    spi_begin_transaction();

    start_byte = spi_transfer_byte(SPI_DUMMY_BYTE);
    text_len   = spi_transfer_byte(SPI_DUMMY_BYTE);

    if (text_len > SPI_MAX_TEXT_LEN) {
        text_len = SPI_MAX_TEXT_LEN;
    }

    for (i = 0; i < text_len; i++) {
        spi_rx_text[i] = (char)spi_transfer_byte(SPI_DUMMY_BYTE);
    }

    spi_end_transaction();

    spi_rx_text[text_len] = '\0';

    printf("SPI: start byte received = 0x%02X\n", start_byte);
    printf("SPI: length received     = %u\n", text_len);
    printf("SPI: text received       = %s\n", spi_rx_text);
    fflush(stdout);

    if (start_byte == SPI_START_BYTE && text_len > 0) {
        sdram_store_text(spi_rx_text);

        printf("SPI: text stored into SDRAM at 0x%08X\n", TEXT_BUFFER_BASE);
        printf("SPI: STATUS_TEXT_READY set to 1\n");
        fflush(stdout);
    } else {
        printf("SPI: invalid packet, text not stored\n");
        fflush(stdout);

        sdram_write_status(STATUS_LAST_ERROR, 1);
    }
}

const char *spi_get_latest_text(void)
{
    return spi_rx_text;
}
