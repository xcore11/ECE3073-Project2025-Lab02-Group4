#include "io.h"
#include <unistd.h>
#include <stdint.h>
#include <string.h>

/* =========================
   Base addresses
   ========================= */
#define SPI_BASE        0x08011020
#define PIOSW_BASE      0x080111C0
#define PIOPB_BASE      0x080111B0

#define HEX0_BASE       0x08011190
#define HEX1_BASE       0x08011180
#define HEX2_BASE       0x08011170
#define HEX3_BASE       0x08011160
#define HEX4_BASE       0x08011150
#define HEX5_BASE       0x08011140

/* =========================
   SPI offsets
   ========================= */
#define SPI_RXDATA      0
#define SPI_TXDATA      4
#define SPI_STATUS      8

/* =========================
   SPI status bits
   ========================= */
#define TRDY            (1 << 6)
#define RRDY            (1 << 7)

/* =========================
   Switch / PB masks
   ========================= */
#define SW1_MASK        (1 << 0)
#define SW2_MASK        (1 << 1)
#define SW3_MASK        (1 << 2)
#define SW4_MASK        (1 << 3)

#define PB1_MASK        (1 << 1)

/* =========================
   Switch polarity
   1 = switch ON means bit = 1
   0 = switch ON means bit = 0
   ========================= */
#define SW_ACTIVE_HIGH  1

/* =========================
   8-bit active-low 7-seg
   with decimal point OFF
   ========================= */
#define SEG_0           0xC0
#define SEG_1           0xF9
#define SEG_2           0xA4
#define SEG_3           0xB0
#define SEG_4           0x99
#define SEG_5           0x92
#define SEG_6           0x82
#define SEG_7           0xF8
#define SEG_8           0x80
#define SEG_9           0x90

#define SEG_A           0x88
#define SEG_B           0x83
#define SEG_C           0xC6
#define SEG_D           0xA1
#define SEG_E           0x86
#define SEG_F           0x8E
#define SEG_H           0x89
#define SEG_I           0xF9
#define SEG_L           0xC7
#define SEG_N           0xAB
#define SEG_O           0xC0
#define SEG_P           0x8C
#define SEG_R           0xAF
#define SEG_S           0x92
#define SEG_U           0xC1
#define SEG_Y           0x91
#define SEG_DASH        0xBF
#define SEG_BLANK       0xFF

/* =========================
   Settings
   ========================= */
#define CPU_UTIL_TEXT   "CPU67 "
#define MSG_LEN         6
#define SCROLL_MAX      64

/* Predefined SPI text */
static const char tx_message[MSG_LEN + 1] = "HELLO ";
static char rx_message[MSG_LEN + 1] = "      ";
static int spi_valid = 0;

/* SPI transaction state */
static char rx_work[MSG_LEN];
static int spi_busy = 0;
static int waiting_rx = 0;
static int tx_index = 0;
static int rx_index = 0;
static int mismatch = 0;

/* =========================
   HEX helpers
   ========================= */
static void hex_write_raw(uint32_t base, uint8_t code)
{
    IOWR_32DIRECT(base, 0, code);
}

static void hex_clear_all(void)
{
    hex_write_raw(HEX5_BASE, SEG_BLANK);
    hex_write_raw(HEX4_BASE, SEG_BLANK);
    hex_write_raw(HEX3_BASE, SEG_BLANK);
    hex_write_raw(HEX2_BASE, SEG_BLANK);
    hex_write_raw(HEX1_BASE, SEG_BLANK);
    hex_write_raw(HEX0_BASE, SEG_BLANK);
}

static uint8_t char_to_7seg(char c)
{
    switch (c) {
        case '0': return SEG_0;
        case '1': return SEG_1;
        case '2': return SEG_2;
        case '3': return SEG_3;
        case '4': return SEG_4;
        case '5': return SEG_5;
        case '6': return SEG_6;
        case '7': return SEG_7;
        case '8': return SEG_8;
        case '9': return SEG_9;

        case 'A': case 'a': return SEG_A;
        case 'B': case 'b': return SEG_B;
        case 'C': case 'c': return SEG_C;
        case 'D': case 'd': return SEG_D;
        case 'E': case 'e': return SEG_E;
        case 'F': case 'f': return SEG_F;
        case 'H': case 'h': return SEG_H;
        case 'I': case 'i': return SEG_I;
        case 'L': case 'l': return SEG_L;
        case 'N': case 'n': return SEG_N;
        case 'O': case 'o': return SEG_O;
        case 'P': case 'p': return SEG_P;
        case 'R': case 'r': return SEG_R;
        case 'S': case 's': return SEG_S;
        case 'U': case 'u': return SEG_U;
        case 'Y': case 'y': return SEG_Y;

        case '-': return SEG_DASH;
        case ' ': return SEG_BLANK;
        default:  return SEG_BLANK;
    }
}

/* leftmost char -> HEX5, rightmost -> HEX0 */
static void display_6chars(const char *s)
{
    hex_write_raw(HEX5_BASE, char_to_7seg(s[0]));
    hex_write_raw(HEX4_BASE, char_to_7seg(s[1]));
    hex_write_raw(HEX3_BASE, char_to_7seg(s[2]));
    hex_write_raw(HEX2_BASE, char_to_7seg(s[3]));
    hex_write_raw(HEX1_BASE, char_to_7seg(s[4]));
    hex_write_raw(HEX0_BASE, char_to_7seg(s[5]));
}

static void display_on_word(void)
{
    display_6chars("  ON  ");
}

/* =========================
   Input helpers
   ========================= */
static uint32_t read_switches(void)
{
    return IORD_32DIRECT(PIOSW_BASE, 0);
}

static uint32_t read_pushbuttons(void)
{
    return IORD_32DIRECT(PIOPB_BASE, 0);
}

static int switch_is_on(uint32_t mask)
{
#if SW_ACTIVE_HIGH
    return (read_switches() & mask) != 0;
#else
    return (read_switches() & mask) == 0;
#endif
}

static int sw1_on(void) { return switch_is_on(SW1_MASK); }
static int sw2_on(void) { return switch_is_on(SW2_MASK); }
static int sw3_on(void) { return switch_is_on(SW3_MASK); }

static int pb1_pressed(void)
{
    return ((read_pushbuttons() & PB1_MASK) == 0);
}

/* =========================
   SPI transaction control
   3-wire, 8-bit, non-blocking
   ========================= */
static void start_spi_capture(void)
{
    spi_busy = 1;
    waiting_rx = 0;
    tx_index = 0;
    rx_index = 0;
    mismatch = 0;
}

static void service_spi(void)
{
    uint32_t status;

    if (!spi_busy) {
        return;
    }

    status = IORD_32DIRECT(SPI_BASE, SPI_STATUS);

    if (!waiting_rx) {
        if (status & TRDY) {
            IOWR_32DIRECT(SPI_BASE, SPI_TXDATA, (uint8_t)tx_message[tx_index]);
            waiting_rx = 1;
        }
    } else {
        if (status & RRDY) {
            uint8_t rx = (uint8_t)(IORD_32DIRECT(SPI_BASE, SPI_RXDATA) & 0xFF);

            rx_work[rx_index] = (char)rx;
            if ((char)rx != tx_message[rx_index]) {
                mismatch = 1;
            }

            rx_index++;
            tx_index++;
            waiting_rx = 0;

            if (tx_index >= MSG_LEN) {
                spi_busy = 0;

                if (!mismatch && rx_index == MSG_LEN) {
                    memcpy(rx_message, rx_work, MSG_LEN);
                    rx_message[MSG_LEN] = '\0';
                    spi_valid = 1;
                } else {
                    spi_valid = 0;
                }
            }
        }
    }
}

/* =========================
   Scrolling helpers
   ========================= */
static void build_scroll_text(char *dest)
{
    int pos = 0;
    const char *base_text;

    memset(dest, ' ', SCROLL_MAX);
    dest[0] = '\0';

    /* leading spaces */
    dest[pos++] = ' ';
    dest[pos++] = ' ';
    dest[pos++] = ' ';
    dest[pos++] = ' ';
    dest[pos++] = ' ';
    dest[pos++] = ' ';

    if (spi_valid) {
        base_text = rx_message;
    } else {
        base_text = "ERROR ";
    }

    /* main message */
    memcpy(&dest[pos], base_text, 6);
    pos += 6;

    /* optional CPU part in same scroll */
    if (sw3_on()) {
        memcpy(&dest[pos], CPU_UTIL_TEXT, 6);
        pos += 6;
    }

    /* trailing spaces */
    dest[pos++] = ' ';
    dest[pos++] = ' ';
    dest[pos++] = ' ';
    dest[pos++] = ' ';
    dest[pos++] = ' ';
    dest[pos++] = ' ';

    dest[pos] = '\0';
}

static int scroll_text_len(const char *s)
{
    int n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

static void display_scroll_window(const char *src, int len, int offset)
{
    char win[6];
    int i;

    for (i = 0; i < 6; i++) {
        int idx = offset + i;
        if (idx < len) {
            win[i] = src[idx];
        } else {
            win[i] = ' ';
        }
    }

    display_6chars(win);
}

int main(void)
{
    int prev_pb1 = 0;
    int scroll_tick = 0;
    int scroll_index = 0;
    char scroll_buf[SCROLL_MAX];
    int scroll_len;

    hex_clear_all();

    while (1)
    {
        int pb1_now = pb1_pressed();

        /* Start SPI only on PB1 press edge */
        if (pb1_now && !prev_pb1) {
            if (!spi_busy) {
                start_spi_capture();
            }
        }
        prev_pb1 = pb1_now;

        /* Non-blocking SPI service */
        service_spi();

        /* SW1 OFF => everything disabled */
        if (!sw1_on()) {
            hex_clear_all();
            scroll_index = 0;
            usleep(50000);
            continue;
        }

        /* SW1 ON only => show ON */
        if (!sw2_on() && !sw3_on()) {
            display_on_word();
            scroll_index = 0;
            usleep(50000);
            continue;
        }

        /* If SW2 ON, scroll message/error,
           and include CPU67 inside same scroll when SW3 ON */
        if (sw2_on()) {
            build_scroll_text(scroll_buf);
            scroll_len = scroll_text_len(scroll_buf);

            scroll_tick++;
            if (scroll_tick >= 4) {   /* about 200 ms */
                scroll_tick = 0;
                display_scroll_window(scroll_buf, scroll_len, scroll_index);
                scroll_index++;

                if (scroll_index > scroll_len - 6) {
                    scroll_index = 0;
                }
            }

            usleep(50000);
            continue;
        }

        /* SW2 OFF, SW3 ON => just show CPU67 steady */
        if (sw3_on()) {
            display_6chars(" CPU67");
            scroll_index = 0;
            usleep(50000);
            continue;
        }

        usleep(50000);
    }

    return 0;
}
