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

#define SPI_SLAVE_0             0x1
#define SPI_DUMMY_BYTE          0x00

/*
   Packet from ESP:
   [0]      magic0 = 'G'
   [1]      magic1 = 'V'
   [2]      version
   [3]      status
   [4..7]   total packet length, little endian, including header
   [8..9]   text length
   [10..11] image width
   [12..13] image height
   [14..15] line count
   [16..19] image length
   [20..21] header length
   [22..23] flags
   [24..]   text bytes, then grayscale image bytes
*/
#define SPI_PACKET_HEADER_SIZE  24
#define SPI_PACKET_MAGIC0       'G'
#define SPI_PACKET_MAGIC1       'V'

#ifndef IMAGE_BUFFER_BASE
#define IMAGE_BUFFER_BASE       0x0520A000
#endif

#ifndef IMAGE_BUFFER_SIZE
#define IMAGE_BUFFER_SIZE       32768
#endif

static char spi_rx_text[TEXT_BUFFER_SIZE];
static uint32_t latest_packet_length = 0;

/* Forward declarations. */
static uint8_t sdram_read_packet_byte(uint32_t index);

/* =========================
   Little endian helpers
   ========================= */

static uint16_t read_u16_le(const uint8_t *p)
{
    return ((uint16_t)p[0]) |
           ((uint16_t)p[1] << 8);
}

static uint32_t read_u32_le(const uint8_t *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint32_t round_up_4(uint32_t value)
{
    return (value + 3u) & ~3u;
}

/* =========================
   SDRAM helpers
   ========================= */

static void sdram_write_status(uint32_t offset, uint32_t value)
{
    /* Internal 32-bit status registers live at STATUS_REG_BASE. */
    IOWR_32DIRECT(STATUS_REG_BASE, offset, value);
}

static void sdram_clear_text(void)
{
    uint32_t i;

    for (i = 0; i < TEXT_BUFFER_SIZE; i++) {
        IOWR_8DIRECT(TEXT_BUFFER_BASE, i, 0);
        spi_rx_text[i] = '\0';
    }
}

static void sdram_store_text_from_packet(uint32_t text_src_offset, uint16_t text_len)
{
    uint32_t i;

    sdram_clear_text();

    if (text_len >= TEXT_BUFFER_SIZE) {
        text_len = TEXT_BUFFER_SIZE - 1;
    }

    for (i = 0; i < text_len; i++) {
        char c = (char)sdram_read_packet_byte(text_src_offset + i);

        /* Store display-safe text only. */
        if (c == '\0' || c == '\r' || c == '\n' || c == '\t') {
            c = ' ';
        }
        if (c < 32 || c > 126) {
            c = ' ';
        }

        IOWR_8DIRECT(TEXT_BUFFER_BASE, i, (uint8_t)c);
        spi_rx_text[i] = c;
    }

    IOWR_8DIRECT(TEXT_BUFFER_BASE, text_len, 0);
    spi_rx_text[text_len] = '\0';

    sdram_write_status(STATUS_SPI_RX_COUNT, (uint32_t)text_len);
    sdram_write_status(STATUS_TEXT_READY, text_len > 0 ? 1 : 0);
}

static void sdram_store_packet_byte(uint32_t index, uint8_t value)
{
    IOWR_8DIRECT(SPI_PACKET_BASE, index, value);
}

static uint8_t sdram_read_packet_byte(uint32_t index)
{
    return (uint8_t)(IORD_8DIRECT(SPI_PACKET_BASE, index) & 0xFF);
}

static void sdram_clear_packet_region(uint32_t count)
{
    uint32_t i;

    if (count > SPI_PACKET_MAX_SIZE) {
        count = SPI_PACKET_MAX_SIZE;
    }

    for (i = 0; i < count; i++) {
        IOWR_8DIRECT(SPI_PACKET_BASE, i, 0);
    }
}

static void sdram_clear_status_header(void)
{
    uint32_t i;

    for (i = 0; i < SPI_PACKET_HEADER_SIZE; i++) {
        IOWR_8DIRECT(STATUS_BASE, i, 0);
    }
}

static void sdram_store_status_header_from_packet(void)
{
    uint32_t i;

    for (i = 0; i < SPI_PACKET_HEADER_SIZE; i++) {
        IOWR_8DIRECT(STATUS_BASE, i, sdram_read_packet_byte(i));
    }
}

static void sdram_store_image_byte(uint32_t index, uint8_t value)
{
    IOWR_8DIRECT(IMAGE_BUFFER_BASE, index, value);
}

static void sdram_clear_image_region(uint32_t count)
{
    uint32_t i;

    if (count > IMAGE_BUFFER_SIZE) {
        count = IMAGE_BUFFER_SIZE;
    }

    for (i = 0; i < count; i++) {
        IOWR_8DIRECT(IMAGE_BUFFER_BASE, i, 0);
    }
}

/* =========================
   Basic SPI helpers
   ========================= */

void spi_init_manual(void)
{
    IOWR_32DIRECT(SPI_BASE, SPI_SLAVESEL_OFS, SPI_SLAVE_0);
    IOWR_32DIRECT(SPI_BASE, SPI_CONTROL_OFS, 0);

    latest_packet_length = 0;

    sdram_clear_text();
    sdram_clear_packet_region(256);
    sdram_clear_status_header();
    sdram_clear_image_region(IMAGE_BUFFER_SIZE);

    sdram_write_status(STATUS_TEXT_READY, 0);
    sdram_write_status(STATUS_CMD_READY, 0);
    sdram_write_status(STATUS_CMD_ACK, 0);
    sdram_write_status(STATUS_SPI_RX_COUNT, 0);
    sdram_write_status(STATUS_LAST_ERROR, 0);
    sdram_write_status(STATUS_PACKET_READY, 0);
    sdram_write_status(STATUS_PACKET_LENGTH, 0);
    sdram_write_status(STATUS_IMAGE_LENGTH, 0);
    sdram_write_status(STATUS_TEXT_LENGTH, 0);
    sdram_write_status(STATUS_LINE_COUNT, 0);
    sdram_write_status(STATUS_IMAGE_WIDTH, 0);
    sdram_write_status(STATUS_IMAGE_HEIGHT, 0);
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
   Packet metadata + text/image store
   ========================= */

static int store_text_and_metadata_from_packet(uint32_t packet_len)
{
    uint8_t header[SPI_PACKET_HEADER_SIZE];
    uint32_t i;
    uint32_t total_len;
    uint16_t text_len;
    uint16_t image_w;
    uint16_t image_h;
    uint16_t line_count;
    uint32_t image_len;
    uint16_t header_len;
    uint8_t status;

    if (packet_len < SPI_PACKET_HEADER_SIZE) {
        printf("SPI error: packet too short\n");
        sdram_write_status(STATUS_LAST_ERROR, 2);
        return 0;
    }

    for (i = 0; i < SPI_PACKET_HEADER_SIZE; i++) {
        header[i] = sdram_read_packet_byte(i);
    }

    if (header[0] != SPI_PACKET_MAGIC0 || header[1] != SPI_PACKET_MAGIC1) {
        printf("SPI error: bad packet magic\n");
        sdram_write_status(STATUS_LAST_ERROR, 3);
        return 0;
    }

    status     = header[3];
    total_len  = read_u32_le(&header[4]);
    text_len   = read_u16_le(&header[8]);
    image_w    = read_u16_le(&header[10]);
    image_h    = read_u16_le(&header[12]);
    line_count = read_u16_le(&header[14]);
    image_len  = read_u32_le(&header[16]);
    header_len = read_u16_le(&header[20]);

    if (header_len == 0) {
        header_len = SPI_PACKET_HEADER_SIZE;
    }

    if (total_len != packet_len) {
        printf("SPI error: packet length mismatch\n");
        sdram_write_status(STATUS_LAST_ERROR, 4);
        return 0;
    }

    if ((uint32_t)header_len + (uint32_t)text_len + image_len > packet_len) {
        printf("SPI error: invalid text/image length\n");
        sdram_write_status(STATUS_LAST_ERROR, 5);
        return 0;
    }

    /* Copy packet header for VGA/status readers. */
    sdram_store_status_header_from_packet();

    /* Store received sentence into TEXT_BUFFER_BASE. */
    sdram_store_text_from_packet(header_len, text_len);

    /* Store raw grayscale image into IMAGE_BUFFER_BASE. */
    sdram_clear_image_region(IMAGE_BUFFER_SIZE);

    if (image_len > 0) {
        uint32_t image_src_offset = (uint32_t)header_len + (uint32_t)text_len;
        uint32_t copy_len = image_len;

        if (copy_len > IMAGE_BUFFER_SIZE) {
            copy_len = IMAGE_BUFFER_SIZE;
        }

        for (i = 0; i < copy_len; i++) {
            sdram_store_image_byte(i, sdram_read_packet_byte(image_src_offset + i));
        }
    }

    sdram_write_status(STATUS_PACKET_READY, 1);
    sdram_write_status(STATUS_PACKET_LENGTH, packet_len);
    sdram_write_status(STATUS_TEXT_LENGTH, text_len);
    sdram_write_status(STATUS_IMAGE_WIDTH, image_w);
    sdram_write_status(STATUS_IMAGE_HEIGHT, image_h);
    sdram_write_status(STATUS_LINE_COUNT, line_count);
    sdram_write_status(STATUS_IMAGE_LENGTH, image_len);
    sdram_write_status(STATUS_LAST_ERROR, status == 1 ? 0 : status);

    printf("Received sentence: %s\n", spi_rx_text);
    printf("Image stored: %ux%u, %lu bytes\n",
           image_w,
           image_h,
           (unsigned long)image_len);
    fflush(stdout);

    return 1;
}

/* =========================
   ESP DMA SPI command protocol
   ========================= */

#define SPI_CMD_SESSION_START       0x5F
#define SPI_CMD_READ_LENGTH         0xA1
#define SPI_CMD_READ_PACKET         0xA2
#define SPI_CMD_ABORT_PACKET        0xA3

#define SPI_COMMAND_BYTES           4
#define SPI_LEN_READ_BYTES          8
#define SPI_PACKET_READ_EXTRA_BYTES 4

#define SPI_LENGTH_RETRY_COUNT      5
#define SPI_LENGTH_RETRY_DELAY_US   20000
#define SPI_ESP_QUEUE_DELAY_US      50000

static void spi_send_command4(uint8_t cmd)
{
    uint32_t i;

    spi_begin_transaction();
    spi_transfer_byte(cmd);
    for (i = 1; i < SPI_COMMAND_BYTES; i++) {
        spi_transfer_byte(SPI_DUMMY_BYTE);
    }
    spi_end_transaction();
}

void spi_send_session_start(void)
{
	printf("SPI debug: sending 0x5F session-start command to ESP");
    spi_send_command4(SPI_CMD_SESSION_START);

    /* Give ESP SPI task time to process the start command before packet IRQs. */
    usleep(SPI_ESP_QUEUE_DELAY_US);
}

static uint32_t spi_read_packet_length_once(void)
{
    uint8_t len_bytes[SPI_LEN_READ_BYTES];
    uint32_t i;
    uint32_t len;

    for (i = 0; i < SPI_LEN_READ_BYTES; i++) {
        len_bytes[i] = 0;
    }

    /*
       Phase 1: send command only.
       The ESP receives 0xA1, then queues the length reply transaction.
    */
    spi_send_command4(SPI_CMD_READ_LENGTH);

    /*
       ESP32 SPI slave DMA needs time to finish the command transaction
       and queue the next DMA transaction. Too short gives 0xFFFFFFFF.
    */
    usleep(SPI_ESP_QUEUE_DELAY_US);

    /* Phase 2: clock the length reply. */
    spi_begin_transaction();
    for (i = 0; i < SPI_LEN_READ_BYTES; i++) {
        len_bytes[i] = spi_transfer_byte(SPI_DUMMY_BYTE);
    }
    spi_end_transaction();

    len = read_u32_le(len_bytes);

    printf("SPI debug: raw length bytes = %02X %02X %02X %02X %02X %02X %02X %02X, len=%lu\n",
           len_bytes[0], len_bytes[1], len_bytes[2], len_bytes[3],
           len_bytes[4], len_bytes[5], len_bytes[6], len_bytes[7],
           (unsigned long)len);

    if (len == 0xFFFFFFFFu) {
        usleep(SPI_LENGTH_RETRY_DELAY_US);
    }

    return len;
}

static void spi_read_packet_bytes(uint32_t packet_len)
{
    uint32_t padded_packet_len;
    uint32_t clocks_to_generate;
    uint32_t i;
    uint8_t first_bytes[8];

    for (i = 0; i < 8; i++) {
        first_bytes[i] = 0;
    }

    padded_packet_len = round_up_4(packet_len);
    clocks_to_generate = padded_packet_len + SPI_PACKET_READ_EXTRA_BYTES;

    printf("SPI debug: sending 0xA2 packet request, expecting %lu bytes\n",
           (unsigned long)packet_len);

    /* Phase 1: tell ESP we want the packet. */
    spi_send_command4(SPI_CMD_READ_PACKET);

    /* Let ESP queue the packet reply before FPGA starts clocking bytes. */
    usleep(SPI_ESP_QUEUE_DELAY_US);

    /* Phase 2: clock packet bytes from ESP. */
    spi_begin_transaction();
    for (i = 0; i < clocks_to_generate; i++) {
        uint8_t rx = spi_transfer_byte(SPI_DUMMY_BYTE);

        if (i < packet_len) {
            sdram_store_packet_byte(i, rx);
        }

        if (i < 8) {
            first_bytes[i] = rx;
        }
    }
    spi_end_transaction();

    printf("SPI debug: first packet bytes = %02X %02X %02X %02X %02X %02X %02X %02X\n",
           first_bytes[0], first_bytes[1], first_bytes[2], first_bytes[3],
           first_bytes[4], first_bytes[5], first_bytes[6], first_bytes[7]);
}

static void spi_abort_packet_on_esp(void)
{
    printf("SPI debug: sending 0xA3 abort/reset packet command to ESP\n");
    spi_send_command4(SPI_CMD_ABORT_PACKET);
    usleep(SPI_ESP_QUEUE_DELAY_US);
}

/* =========================
   Main SPI request function
   ========================= */

void spi_request_text_from_esp(void)
{
    uint32_t packet_len;
    uint32_t attempt;

    latest_packet_length = 0;
    sdram_write_status(STATUS_TEXT_READY, 0);
    sdram_write_status(STATUS_PACKET_READY, 0);
    sdram_write_status(STATUS_LAST_ERROR, 0);

    /*
       Realtime mode:
       No 0x5F trigger and no 5 second wait.
       ESP_DATA_READY already means the ESP has a packet prepared.
       FPGA only clocks SPI to read length + packet.
    */
    packet_len = 0;

    for (attempt = 1; attempt <= SPI_LENGTH_RETRY_COUNT; attempt++) {
        packet_len = spi_read_packet_length_once();
        latest_packet_length = packet_len;

        if (packet_len != 0 && packet_len != 0xFFFFFFFFu) {
            break;
        }

        usleep(SPI_LENGTH_RETRY_DELAY_US);
    }

    if (packet_len == 0) {
        printf("SPI error: ESP_DATA_READY high but packet length is 0\n");
        sdram_write_status(STATUS_LAST_ERROR, 6);
        latest_packet_length = 0;
        spi_abort_packet_on_esp();
        return;
    }

    if (packet_len == 0xFFFFFFFFu) {
        static uint32_t invalid_ff_count = 0;

        invalid_ff_count++;

        if ((invalid_ff_count % 20) == 1) {
            printf("SPI waiting: invalid length 0xFFFFFFFF, ESP reply not queued yet\n");
        }

        sdram_write_status(STATUS_LAST_ERROR, 8);
        latest_packet_length = 0;
        spi_abort_packet_on_esp();

        /* Prevent tight retry spam while ESP_DATA_READY is still high. */
        usleep(50000);
        return;
    }

    if (packet_len > SPI_PACKET_MAX_SIZE) {
        printf("SPI error: packet too large\n");
        sdram_write_status(STATUS_LAST_ERROR, 7);
        latest_packet_length = 0;
        spi_abort_packet_on_esp();
        return;
    }

    sdram_clear_packet_region(packet_len + SPI_PACKET_READ_EXTRA_BYTES);
    spi_read_packet_bytes(packet_len);

    if (!store_text_and_metadata_from_packet(packet_len)) {
        latest_packet_length = 0;
        spi_abort_packet_on_esp();
        return;
    }

    latest_packet_length = packet_len;
}

const char *spi_get_latest_text(void)
{
    return spi_rx_text;
}

uint32_t spi_get_latest_packet_length(void)
{
    return latest_packet_length;
}
