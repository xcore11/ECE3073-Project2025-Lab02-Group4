#include "switches.h"
#include "io.h"
#include <stdint.h>
#include <string.h>
#include <unistd.h>

// SPI register base address
#define SPI_BASE            0x08011020

// Offsets for each SPI register
#define SPI_RXDATA_OFS      0
#define SPI_TXDATA_OFS      4
#define SPI_STATUS_OFS      8
#define SPI_CONTROL_OFS     12
#define SPI_SLAVESEL_OFS    20

// Status bits from SPI status register
#define SPI_STATUS_RRDY     (1 << 7)   // receive data ready
#define SPI_STATUS_TRDY     (1 << 6)   // transmit ready
#define SPI_STATUS_TMT      (1 << 5)   // transmit empty
#define SPI_STATUS_TOE      (1 << 4)   // transmit overflow error
#define SPI_STATUS_ROE      (1 << 3)   // receive overflow error
#define SPI_STATUS_E        (1 << 8)   // generic error

// Control bit to hold slave select active
#define SPI_CONTROL_SSO     (1 << 10)

// General SPI settings
#define SPI_TIMEOUT         1000000UL
#define SPI_SLAVE_0         0x1
#define ESP_REPLY_WAIT_US   5200000UL

// Maximum length of message
#define SPI_MAX_MSG_LEN     64

// Stores whether latest received message is valid
static int spi_valid = 0;

// Stores received SPI message
static char rx_message[SPI_MAX_MSG_LEN + 1];

// Clears SPI error flags
static void spi_clear_errors(void)
{
    IOWR_32DIRECT(SPI_BASE, SPI_STATUS_OFS, 0);
}

// Wait until transmit register is ready
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

// Wait until receive data is ready
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

// Wait until transmitter is fully empty
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

// Starts an SPI transaction by selecting slave 0
static void spi_begin_transaction(void)
{
    IOWR_32DIRECT(SPI_BASE, SPI_SLAVESEL_OFS, SPI_SLAVE_0);
    IOWR_32DIRECT(SPI_BASE, SPI_CONTROL_OFS, SPI_CONTROL_SSO);
}

// Ends the transaction after making sure last byte finished sending
static void spi_end_transaction(void)
{
    spi_wait_tmt();
    IOWR_32DIRECT(SPI_BASE, SPI_CONTROL_OFS, 0);
}

// Sends one byte and reads one byte back
static int spi_transfer_byte(uint8_t tx, uint8_t *rx)
{
    uint32_t status;

    // wait until SPI can accept a new byte to transmit
    if (!spi_wait_trdy()) {
        return 0;
    }

    // write transmit byte into TX register
    IOWR_32DIRECT(SPI_BASE, SPI_TXDATA_OFS, tx);

    // wait until a byte comes back
    if (!spi_wait_rrdy()) {
        return 0;
    }

    // read only the lowest 8 bits from RX register
    *rx = (uint8_t)(IORD_32DIRECT(SPI_BASE, SPI_RXDATA_OFS) & 0xFF);

    // check if any SPI error happened
    status = IORD_32DIRECT(SPI_BASE, SPI_STATUS_OFS);
    if (status & (SPI_STATUS_TOE | SPI_STATUS_ROE | SPI_STATUS_E)) {
        spi_clear_errors();
        return 0;
    }

    return 1;
}

// Initializes SPI software state and clears buffers
void spi_init_manual(void)
{
    spi_valid = 0;
    memset(rx_message, 0, sizeof(rx_message));

    spi_clear_errors();
    IOWR_32DIRECT(SPI_BASE, SPI_SLAVESEL_OFS, SPI_SLAVE_0);
    IOWR_32DIRECT(SPI_BASE, SPI_CONTROL_OFS, 0);
}

// Sends a message first, then clocks back a reply from slave
void spi_start_capture(const char *message)
{
    int i;
    int msg_len;
    uint8_t throwaway;
    uint8_t rx_byte;
    char tx_message[SPI_MAX_MSG_LEN + 1];

    // reset previous result
    spi_valid = 0;
    memset(rx_message, 0, sizeof(rx_message));

    if (message == 0) {
        return;
    }

    // copy the message safely into local buffer
    strncpy(tx_message, message, SPI_MAX_MSG_LEN);
    tx_message[SPI_MAX_MSG_LEN] = '\0';
    msg_len = strlen(tx_message);

    if (msg_len <= 0) {
        return;
    }

    spi_clear_errors();

    // first transaction: send the whole message to slave
    spi_begin_transaction();

    for (i = 0; i < msg_len; i++) {
        // received byte is ignored here, so store in throwaway
        if (!spi_transfer_byte((uint8_t)tx_message[i], &throwaway)) {
            spi_end_transaction();
            return;
        }
    }

    spi_end_transaction();

    // optional wait in case slave needs time to prepare reply
    // usleep(ESP_REPLY_WAIT_US);

    spi_clear_errors();

    // second transaction: send dummy bytes to read reply back
    spi_begin_transaction();

    for (i = 0; i < msg_len; i++) {
        // sending 0x00 just generates clock so slave can return data
        if (!spi_transfer_byte(0x00, &rx_byte)) {
            spi_end_transaction();
            return;
        }
        rx_message[i] = (char)rx_byte;
    }

    spi_end_transaction();

    // add null terminator so rx_message becomes a proper C string
    rx_message[msg_len] = '\0';

    // mark valid only if reply matches original message
    if (memcmp(rx_message, tx_message, msg_len) == 0) {
        spi_valid = 1;
    }
}

// Not used for now because this SPI code is blocking
int spi_is_busy(void)
{
    return 0;
}

// Returns whether latest message received is valid
int spi_has_valid_message(void)
{
    return spi_valid;
}

// Copies received message into user buffer
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
