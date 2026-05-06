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
#define SPI_REQUEST_BYTE        0x5F
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
    /*
       Internal 32-bit status registers live at STATUS_REG_BASE.
       Do not write these into STATUS_BASE, because STATUS_BASE is the
       24-byte packet header area that VGA reads.
    */
    IOWR_32DIRECT(STATUS_REG_BASE, offset, value);
}

static void sdram_clear_text(void)
{
    volatile char *dst = (volatile char *)TEXT_BUFFER_BASE;
    uint32_t i;

    for (i = 0; i < TEXT_BUFFER_SIZE; i++) {
        dst[i] = '\0';
    }

    for (i = 0; i < TEXT_BUFFER_SIZE; i++) {
        spi_rx_text[i] = '\0';
    }
}

static void sdram_store_text_bytes(const uint8_t *text, uint16_t text_len)
{
    volatile char *dst = (volatile char *)TEXT_BUFFER_BASE;
    uint32_t i;

    sdram_clear_text();

    if (text_len >= TEXT_BUFFER_SIZE) {
        text_len = TEXT_BUFFER_SIZE - 1;
    }

    for (i = 0; i < text_len; i++) {
        dst[i] = (char)text[i];
        spi_rx_text[i] = (char)text[i];
    }

    dst[text_len] = '\0';
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

    /*
       Copy the first 24 packet bytes to STATUS_BASE.
       VGA reads this and expects STATUS_BASE[0..1] == 'G''V'.
    */
    for (i = 0; i < SPI_PACKET_HEADER_SIZE; i++) {
        IOWR_8DIRECT(STATUS_BASE, i, sdram_read_packet_byte(i));
    }
}


static void sdram_store_image_byte(uint32_t index, uint8_t value)
{
    IOWR_8DIRECT(IMAGE_BUFFER_BASE, index, value);
}

static uint8_t sdram_read_image_byte(uint32_t index)
{
    return (uint8_t)(IORD_8DIRECT(IMAGE_BUFFER_BASE, index) & 0xFF);
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

static void print_sdram_image_first_last20(uint32_t image_len)
{
    uint32_t i;
    uint32_t start;
    uint32_t first_count;

    printf("========== FPGA SDRAM IMAGE COMPARE ==========\n");
    printf("IMAGE_BUFFER_BASE=0x%08X\n", IMAGE_BUFFER_BASE);
    printf("image_len=%lu\n", (unsigned long)image_len);

    if (image_len == 0) {
        printf("first20=<none>\n");
        printf("last20=<none>\n");
        printf("==============================================\n");
        fflush(stdout);
        return;
    }

    first_count = image_len < 20 ? image_len : 20;

    printf("first20=");
    for (i = 0; i < first_count; i++) {
        printf("%02X", sdram_read_image_byte(i));
        if (i + 1 < first_count) {
            printf(" ");
        }
    }
    printf("\n");

    start = image_len > 20 ? image_len - 20 : 0;

    printf("last20=");
    for (i = start; i < image_len; i++) {
        printf("%02X", sdram_read_image_byte(i));
        if (i + 1 < image_len) {
            printf(" ");
        }
    }
    printf("\n");
    printf("==============================================\n");
    fflush(stdout);
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
   Packet metadata + text store
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
    uint16_t flags;
    uint8_t status;

    if (packet_len < SPI_PACKET_HEADER_SIZE) {
        printf("SPI: packet too short\n");
        sdram_write_status(STATUS_LAST_ERROR, 2);
        return 0;
    }

    for (i = 0; i < SPI_PACKET_HEADER_SIZE; i++) {
        header[i] = sdram_read_packet_byte(i);
    }

    if (header[0] != SPI_PACKET_MAGIC0 || header[1] != SPI_PACKET_MAGIC1) {
        printf("SPI: bad packet magic 0x%02X 0x%02X\n", header[0], header[1]);
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
    flags      = read_u16_le(&header[22]);

    if (header_len == 0) {
        header_len = SPI_PACKET_HEADER_SIZE;
    }

    if (total_len != packet_len) {
        printf("SPI: length mismatch header=%lu received=%lu\n",
               (unsigned long)total_len,
               (unsigned long)packet_len);
        sdram_write_status(STATUS_LAST_ERROR, 4);
        return 0;
    }

    if ((uint32_t)header_len + (uint32_t)text_len + image_len > packet_len) {
        printf("SPI: invalid text/image lengths\n");
        sdram_write_status(STATUS_LAST_ERROR, 5);
        return 0;
    }

    /*
       Copy the real 24-byte ESP packet header to STATUS_BASE for VGA.
       This is what fixes VGA seeing random magic like 0x0E 0x0C.
    */
    sdram_store_status_header_from_packet();

    /*
       This only copies the already-recognized text bytes from the ESP packet
       into TEXT_BUFFER_BASE. It does not run decoder.c or convert the text
       into peripheral instructions.
    */
    sdram_store_text_bytes((const uint8_t *)(SPI_PACKET_BASE + header_len), text_len);

    /*
       Copy the raw grayscale image payload into a separate SDRAM image buffer
       so VGA/debug code can read it directly without parsing the packet.
    */
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

    print_sdram_image_first_last20(image_len);

    sdram_write_status(STATUS_PACKET_READY, 1);
    sdram_write_status(STATUS_PACKET_LENGTH, packet_len);
    sdram_write_status(STATUS_TEXT_LENGTH, text_len);
    sdram_write_status(STATUS_IMAGE_WIDTH, image_w);
    sdram_write_status(STATUS_IMAGE_HEIGHT, image_h);
    sdram_write_status(STATUS_LINE_COUNT, line_count);
    sdram_write_status(STATUS_IMAGE_LENGTH, image_len);
    sdram_write_status(STATUS_LAST_ERROR, status == 1 ? 0 : status);

    printf("SPI: packet stored only, no instruction decoding\n");
    printf("SPI: packet_len=%lu text_len=%u image=%ux%u image_len=%lu lines=%u status=%u flags=0x%04X\n",
           (unsigned long)packet_len,
           text_len,
           image_w,
           image_h,
           (unsigned long)image_len,
           line_count,
           status,
           flags);
    printf("SPI: text stored at TEXT_BUFFER_BASE=[%s]\n", spi_rx_text);
    printf("SPI: full packet stored at SPI_PACKET_BASE=0x%08X\n", SPI_PACKET_BASE);
    printf("SPI: header copied to STATUS_BASE=0x%08X\n", STATUS_BASE);
    printf("SPI: raw image stored at IMAGE_BUFFER_BASE=0x%08X\n", IMAGE_BUFFER_BASE);
    fflush(stdout);

    return 1;
}

/* =========================
   ESP DMA SPI command protocol
   ========================= */

#define SPI_CMD_TRIGGER_CAPTURE     0x5F
#define SPI_CMD_READ_LENGTH         0xA1
#define SPI_CMD_READ_PACKET         0xA2

#define SPI_COMMAND_BYTES           4
#define SPI_LEN_READ_BYTES          8
#define SPI_PACKET_READ_EXTRA_BYTES 4

#define SPI_LENGTH_RETRY_COUNT      20
#define SPI_LENGTH_RETRY_DELAY_US   500000
#define SPI_ESP_QUEUE_DELAY_US      2000
#define SPI_CAPTURE_WAIT_US         5000000

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

static uint32_t spi_read_packet_length_once(void)
{
    uint8_t len_bytes[SPI_LEN_READ_BYTES];
    uint32_t i;

    for (i = 0; i < SPI_LEN_READ_BYTES; i++) {
        len_bytes[i] = 0;
    }

    /* Command transaction: tell ESP to prepare length reply. */
    spi_send_command4(SPI_CMD_READ_LENGTH);

    /* Give ESP DMA task a tiny window to queue the reply transaction. */
    usleep(SPI_ESP_QUEUE_DELAY_US);

    /* Read transaction: FPGA generates SCLK using dummy bytes. */
    spi_begin_transaction();
    for (i = 0; i < SPI_LEN_READ_BYTES; i++) {
        len_bytes[i] = spi_transfer_byte(SPI_DUMMY_BYTE);
    }
    spi_end_transaction();

    return read_u32_le(len_bytes);
}

static void spi_read_packet_bytes(uint32_t packet_len)
{
    uint32_t padded_packet_len;
    uint32_t clocks_to_generate;
    uint32_t i;

    padded_packet_len = round_up_4(packet_len);
    clocks_to_generate = padded_packet_len + SPI_PACKET_READ_EXTRA_BYTES;

    printf("SPI: reading packet bytes=%lu padded=%lu clocks=%lu\n",
           (unsigned long)packet_len,
           (unsigned long)padded_packet_len,
           (unsigned long)clocks_to_generate);
    fflush(stdout);

    /* Command transaction: tell ESP to queue the packet as DMA MISO data. */
    spi_send_command4(SPI_CMD_READ_PACKET);
    usleep(SPI_ESP_QUEUE_DELAY_US);

    /* Read transaction: FPGA sends dummy bytes only to generate SCLK. */
    spi_begin_transaction();
    for (i = 0; i < clocks_to_generate; i++) {
        uint8_t rx = spi_transfer_byte(SPI_DUMMY_BYTE);

        if (i < packet_len) {
            sdram_store_packet_byte(i, rx);
        }
    }
    spi_end_transaction();
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

    printf("SPI: sending capture trigger command 0x%02X\n", SPI_CMD_TRIGGER_CAPTURE);
    fflush(stdout);

    /*
       Transaction 1:
       FPGA sends 4 command bytes. ESP DMA command task reads byte[0].
       byte[0] = 0x5F, byte[1..3] = dummy padding.
    */
    spi_send_command4(SPI_CMD_TRIGGER_CAPTURE);

    printf("SPI: waiting %lu us for ESP capture/final packet build\n",
           (unsigned long)SPI_CAPTURE_WAIT_US);
    fflush(stdout);
    usleep(SPI_CAPTURE_WAIT_US);

    packet_len = 0;

    for (attempt = 1; attempt <= SPI_LENGTH_RETRY_COUNT; attempt++) {
        printf("SPI: reading packet length attempt %lu/%u\n",
               (unsigned long)attempt,
               SPI_LENGTH_RETRY_COUNT);
        fflush(stdout);

        packet_len = spi_read_packet_length_once();
        latest_packet_length = packet_len;

        printf("SPI: packet length received = %lu\n", (unsigned long)packet_len);
        fflush(stdout);

        if (packet_len != 0 && packet_len != 0xFFFFFFFFu) {
            break;
        }

        if (packet_len == 0xFFFFFFFFu) {
            printf("SPI: 0xFFFFFFFF means MISO high or ESP did not queue length reply. Retrying...\n");
        } else {
            printf("SPI: ESP length=0, packet not ready yet. Retrying...\n");
        }

        usleep(SPI_LENGTH_RETRY_DELAY_US);
    }

    if (packet_len == 0) {
        printf("SPI: ESP packet not ready after retries.\n");
        sdram_write_status(STATUS_LAST_ERROR, 6);
        return;
    }

    if (packet_len == 0xFFFFFFFFu) {
        printf("SPI: still read 0xFFFFFFFF after retries. Check SPI mode/wiring/CS/MISO.\n");
        sdram_write_status(STATUS_LAST_ERROR, 8);
        return;
    }

    if (packet_len > SPI_PACKET_MAX_SIZE) {
        printf("SPI: packet too large, max=%lu\n", (unsigned long)SPI_PACKET_MAX_SIZE);
        sdram_write_status(STATUS_LAST_ERROR, 7);
        return;
    }

    sdram_clear_packet_region(packet_len + SPI_PACKET_READ_EXTRA_BYTES);

    spi_read_packet_bytes(packet_len);

    store_text_and_metadata_from_packet(packet_len);
}

const char *spi_get_latest_text(void)
{
    return spi_rx_text;
}

uint32_t spi_get_latest_packet_length(void)
{
    return latest_packet_length;
}
