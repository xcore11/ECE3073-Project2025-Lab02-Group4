#include "switches.h"
#include "io.h"
#include <stdint.h>

/* SPI base address */
#define SPI_BASE        0x08011020

/* SPI register offsets */
#define SPI_RXDATA      0
#define SPI_TXDATA      4
#define SPI_STATUS      8
#define SPI_CONTROL     12
#define SPI_SLAVESEL    20

/* SPI status bits */
#define SPI_STATUS_TRDY (1 << 6)
#define SPI_STATUS_RRDY (1 << 7)

/* SPI control bits */
#define SPI_CONTROL_SSO (1 << 10)

#define SPI_TIMEOUT     1000000

void fpga_spi_init(void)
{
    IOWR_32DIRECT(SPI_BASE, SPI_SLAVESEL, 0x1);
    IOWR_32DIRECT(SPI_BASE, SPI_CONTROL, 0x0);
}

static void fpga_spi_assert_ss(void)
{
    IOWR_32DIRECT(SPI_BASE, SPI_SLAVESEL, 0x1);
    IOWR_32DIRECT(SPI_BASE, SPI_CONTROL, SPI_CONTROL_SSO);
}

static void fpga_spi_deassert_ss(void)
{
    IOWR_32DIRECT(SPI_BASE, SPI_CONTROL, 0x0);
}

static int fpga_spi_transfer_byte(uint8_t tx, uint8_t *rx)
{
    uint32_t status;
    int timeout;

    timeout = SPI_TIMEOUT;
    do {
        status = IORD_32DIRECT(SPI_BASE, SPI_STATUS);
        timeout--;
        if (timeout <= 0) {
            return -1;
        }
    } while ((status & SPI_STATUS_TRDY) == 0);

    IOWR_32DIRECT(SPI_BASE, SPI_TXDATA, tx);

    timeout = SPI_TIMEOUT;
    do {
        status = IORD_32DIRECT(SPI_BASE, SPI_STATUS);
        timeout--;
        if (timeout <= 0) {
            return -1;
        }
    } while ((status & SPI_STATUS_RRDY) == 0);

    *rx = (uint8_t)(IORD_32DIRECT(SPI_BASE, SPI_RXDATA) & 0xFF);
    return 0;
}

int fpga_spi_transfer_message(const char *tx_buf, char *rx_buf, int len)
{
    int i;
    uint8_t rx_byte;

    if (tx_buf == 0 || rx_buf == 0 || len <= 0 || len > FPGA_SPI_MAX_MSG_LEN) {
        return -1;
    }

    fpga_spi_assert_ss();

    for (i = 0; i < len; i++) {
        if (fpga_spi_transfer_byte((uint8_t)tx_buf[i], &rx_byte) != 0) {
            fpga_spi_deassert_ss();
            return -1;
        }
        rx_buf[i] = (char)rx_byte;
    }

    fpga_spi_deassert_ss();
    return 0;
}

int fpga_spi_transfer_string(const char *tx_msg, char *rx_msg)
{
    int len = 0;

    if (tx_msg == 0 || rx_msg == 0) {
        return -1;
    }

    while (tx_msg[len] != '\0') {
        len++;
        if (len >= FPGA_SPI_MAX_MSG_LEN) {
            return -1;
        }
    }

    if (fpga_spi_transfer_message(tx_msg, rx_msg, len) != 0) {
        return -1;
    }

    rx_msg[len] = '\0';
    return 0;
}
