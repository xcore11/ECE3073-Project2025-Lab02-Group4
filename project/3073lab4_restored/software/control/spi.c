#include "switches.h"
#include "io.h"
#include <stdint.h>
#include <string.h>
#include <unistd.h>

/* =========================
   User-configurable settings
   ========================= */
#define SPI_BASE            0x08011020
#define SPI_RXDATA_OFS      0
#define SPI_TXDATA_OFS      4
#define SPI_STATUS_OFS      8
#define SPI_CONTROL_OFS     12
#define SPI_SLAVESEL_OFS    20

#define SPI_STATUS_RRDY     (1 << 7)
#define SPI_STATUS_TRDY     (1 << 6)
#define SPI_STATUS_TMT      (1 << 5)
#define SPI_STATUS_TOE      (1 << 4)
#define SPI_STATUS_ROE      (1 << 3)
#define SPI_STATUS_E        (1 << 8)

#define SPI_CONTROL_SSO     (1 << 10)

#define SPI_TIMEOUT         1000000UL
#define SPI_SLAVE_0         0x1
#define ESP_REPLY_WAIT_US   5200000UL

/* max message length to send/receive */
#define SPI_MAX_MSG_LEN     64

static int  spi_valid = 0;
static char rx_message[SPI_MAX_MSG_LEN + 1];

/* ---------- low-level helpers ---------- */

static void spi_clear_errors(void)
{
    IOWR_32DIRECT(SPI_BASE, SPI_STATUS_OFS, 0);
}

static int spi_wait_trdy(void)
{
    uint32_t timeout = SPI_TIMEOUT;

    while (timeout--) {
        if (IORD_32DIRECT(SPI_BASE, SPI_STATUS_OFS) & SPI_STATUS_TRDY) {
            return 1;
        }
    }
    return 0;
}

static int spi_wait_rrdy(void)
{
    uint32_t timeout = SPI_TIMEOUT;

    while (timeout--) {
        if (IORD_32DIRECT(SPI_BASE, SPI_STATUS_OFS) & SPI_STATUS_RRDY) {
            return 1;
        }
    }
    return 0;
}

static int spi_wait_tmt(void)
{
    uint32_t timeout = SPI_TIMEOUT;

    while (timeout--) {
        if (IORD_32DIRECT(SPI_BASE, SPI_STATUS_OFS) & SPI_STATUS_TMT) {
            return 1;
        }
    }
    return 0;
}

static void spi_begin_transaction(void)
{
    IOWR_32DIRECT(SPI_BASE, SPI_SLAVESEL_OFS, SPI_SLAVE_0);
    IOWR_32DIRECT(SPI_BASE, SPI_CONTROL_OFS, SPI_CONTROL_SSO);
}

static void spi_end_transaction(void)
{
    spi_wait_tmt();
    IOWR_32DIRECT(SPI_BASE, SPI_CONTROL_OFS, 0);
}

static int spi_transfer_byte(uint8_t tx, uint8_t *rx)
{
    uint32_t status;

    if (!spi_wait_trdy()) {
        return 0;
    }

    IOWR_32DIRECT(SPI_BASE, SPI_TXDATA_OFS, tx);

    if (!spi_wait_rrdy()) {
        return 0;
    }

    *rx = (uint8_t)(IORD_32DIRECT(SPI_BASE, SPI_RXDATA_OFS) & 0xFF);

    status = IORD_32DIRECT(SPI_BASE, SPI_STATUS_OFS);
    if (status & (SPI_STATUS_TOE | SPI_STATUS_ROE | SPI_STATUS_E)) {
        spi_clear_errors();
        return 0;
    }

    return 1;
}

/* ---------- API used by main.c ---------- */

void spi_init_manual(void)
{
    spi_valid = 0;
    memset(rx_message, 0, sizeof(rx_message));

    spi_clear_errors();
    IOWR_32DIRECT(SPI_BASE, SPI_SLAVESEL_OFS, SPI_SLAVE_0);
    IOWR_32DIRECT(SPI_BASE, SPI_CONTROL_OFS, 0);
}

void spi_start_capture(const char *message)
{
    int i;
    int msg_len;
    uint8_t throwaway;
    uint8_t rx_byte;
    char tx_message[SPI_MAX_MSG_LEN + 1];

    spi_valid = 0;
    memset(rx_message, 0, sizeof(rx_message));

    if (message == 0) {
        return;
    }

    /* copy safely and compute actual length */
    strncpy(tx_message, message, SPI_MAX_MSG_LEN);
    tx_message[SPI_MAX_MSG_LEN] = '\0';
    msg_len = strlen(tx_message);

    if (msg_len <= 0) {
        return;
    }

    spi_clear_errors();

    /* Transaction 1: send full message to ESP32 */
    spi_begin_transaction();

    for (i = 0; i < msg_len; i++) {
        if (!spi_transfer_byte((uint8_t)tx_message[i], &throwaway)) {
            spi_end_transaction();
            return;
        }
    }

    spi_end_transaction();

    /* Wait for ESP32 delay(5000) + queue reply */
    // usleep(ESP_REPLY_WAIT_US);

    spi_clear_errors();

    /* Transaction 2: send dummy bytes, read full reply back */
    spi_begin_transaction();

    for (i = 0; i < msg_len; i++) {
        if (!spi_transfer_byte(0x00, &rx_byte)) {
            spi_end_transaction();
            return;
        }
        rx_message[i] = (char)rx_byte;
    }

    spi_end_transaction();

    rx_message[msg_len] = '\0';

    if (memcmp(rx_message, tx_message, msg_len) == 0) {
        spi_valid = 1;
    }
}

void spi_service(void)
{
}

int spi_is_busy(void)
{
    return 0;
}

int spi_has_valid_message(void)
{
    return spi_valid;
}

void spi_get_message(char *buf, int max_len)
{
    int n;

    if (!buf || max_len <= 0) {
        return;
    }

    if (!spi_valid) {
        buf[0] = '\0';
        return;
    }

    n = strlen(rx_message);
    if (n > max_len - 1) {
        n = max_len - 1;
    }

    memcpy(buf, rx_message, n);
    buf[n] = '\0';
}
