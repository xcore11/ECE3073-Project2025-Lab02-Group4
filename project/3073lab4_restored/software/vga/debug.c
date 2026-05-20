#include "io.h"
#include "system.h"
#include "vga.h"
#include "debug.h"

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "altera_avalon_pio_regs.h"

/* =========================
   VGA IRQ RX defines
   Same as old code
   ========================= */

#define VGA_IRQ_RX_BASE 0x9060

#define VGA_IRQ_RX_ACTIVE_MASK 0x01
#define VGA_IRQ_TX_ACTIVE_MASK 0x01

/* =========================
   SDRAM locations written by image processor
   Same as old code
   ========================= */

#define TEXT_BUFFER_BASE    0x05200000
#define STATUS_BASE         0x05209000
#define IMAGE_BUFFER_BASE   0x0520A000

/* =========================
   Image format from ESP
   Same as old code
   ========================= */

#define IMAGE_W             96
#define IMAGE_H             96
#define IMAGE_LEN           (IMAGE_W * IMAGE_H)

/* =========================
   VGA layout
   Same as old code
   ========================= */

#define IMAGE_BOX_X         114
#define IMAGE_BOX_Y         24
#define IMAGE_BOX_W         92
#define IMAGE_BOX_H         92

#define TEXT_REGION_X       0
#define TEXT_REGION_Y       140
#define TEXT_REGION_W       320
#define TEXT_REGION_H       100

#define TEXT_START_X        10
#define TEXT_START_Y        150
#define TEXT_LINE_STEP      14

/* =========================
   VGA colors, 4-bit
   Same as old code
   ========================= */

#define VGA_BLACK           0x0
#define VGA_WHITE           0xF
#define VGA_DARK_BG         0x1
#define VGA_IMAGE_BOX_BG    0xF

#define INVERT_IMAGE_GRAYSCALE 0

/* =========================
   Packet header offsets in STATUS_BASE
   Same as old code
   ========================= */

#define STATUS_MAGIC0_OFS       0
#define STATUS_MAGIC1_OFS       1
#define STATUS_TEXT_LEN_OFS     8

/* =========================
   Little endian readers
   Same as old code
   ========================= */

static uint16_t read_u16_le(volatile uint8_t *p)
{
    return ((uint16_t)p[0]) | ((uint16_t)p[1] << 8);
}

/* =========================
   VGA IRQ TX helpers
   Same as old code
   ========================= */

static void vga_irq_tx_set_true(void)
{
    IOWR_ALTERA_AVALON_PIO_DATA(VGA_IRQ_TX_BASE, VGA_IRQ_TX_ACTIVE_MASK);
}

void debug_irq_tx_set_false(void)
{
    IOWR_ALTERA_AVALON_PIO_DATA(VGA_IRQ_TX_BASE, 0x0);
}

/* =========================
   Status helpers
   Same as old code
   ========================= */

static int packet_ready(void)
{
    volatile uint8_t *status = (volatile uint8_t *)STATUS_BASE;

    return (status[STATUS_MAGIC0_OFS] == 'G' &&
            status[STATUS_MAGIC1_OFS] == 'V');
}

static uint16_t get_text_len(void)
{
    volatile uint8_t *status = (volatile uint8_t *)STATUS_BASE;
    return read_u16_le(&status[STATUS_TEXT_LEN_OFS]);
}

/* =========================
   8-bit grayscale to 4-bit VGA grayscale
   Same as old code
   ========================= */

static uint8_t gray_to_vga_color(uint8_t g)
{
#if INVERT_IMAGE_GRAYSCALE
    g = 255 - g;
#endif

    return (g >> 4) & 0x0F;
}

/* =========================
   Draw image from SDRAM
   Same as old code
   ========================= */

static void draw_image_from_sdram(void)
{
    volatile uint8_t *img = (volatile uint8_t *)IMAGE_BUFFER_BASE;

    int x;
    int y;

    for (y = 0; y < IMAGE_BOX_H; y++)
    {
        int src_y = (y * IMAGE_H) / IMAGE_BOX_H;

        for (x = 0; x < IMAGE_BOX_W; x++)
        {
            int src_x = (x * IMAGE_W) / IMAGE_BOX_W;
            uint8_t gray = img[src_y * IMAGE_W + src_x];
            uint8_t color = gray_to_vga_color(gray);

            vga_draw_rectangle(IMAGE_BOX_X + x,
                               IMAGE_BOX_Y + y,
                               1,
                               1,
                               color);
        }
    }
}

/* =========================
   Draw text from SDRAM
   Same as old code
   ========================= */

static void draw_text_from_sdram(uint16_t text_len)
{
    char line[48];
    int line_pos = 0;
    int screen_line = 0;
    int max_chars = 36;
    int max_lines = 6;
    uint16_t i;

    vga_draw_rectangle(TEXT_REGION_X,
                       TEXT_REGION_Y,
                       TEXT_REGION_W,
                       TEXT_REGION_H,
                       VGA_BLACK);

    if (text_len == 0)
    {
        printf("Displayed text: NO TEXT\n");

        vga_print_software_text(TEXT_START_X,
                                TEXT_START_Y,
                                "NO TEXT",
                                VGA_WHITE);
        return;
    }

    printf("Displayed text:\n");

    memset(line, 0, sizeof(line));

    for (i = 0; i < text_len; i++)
    {
        char c = (char)(IORD_8DIRECT(TEXT_BUFFER_BASE, i) & 0xFF);

        if (c == '\0' || c == '\r' || c == '\n' || c == '\t')
        {
            c = ' ';
        }
        if (c < 32 || c > 126)
        {
            c = ' ';
        }

        line[line_pos++] = c;

        if (line_pos >= max_chars || i == text_len - 1)
        {
            line[line_pos] = '\0';

            printf("%s\n", line);

            vga_print_software_text(TEXT_START_X,
                                    TEXT_START_Y + screen_line * TEXT_LINE_STEP,
                                    line,
                                    VGA_WHITE);

            screen_line++;
            line_pos = 0;
            memset(line, 0, sizeof(line));

            if (screen_line >= max_lines)
            {
                break;
            }
        }
    }

    fflush(stdout);
}

/* =========================
   Draw static screen
   Same as old code
   ========================= */

static void draw_static_screen(void)
{
    vga_draw_rectangle(0, 0, 320, 140, VGA_DARK_BG);
    vga_draw_rectangle(0, 140, 320, 100, VGA_BLACK);

    vga_draw_rectangle(IMAGE_BOX_X,
                       IMAGE_BOX_Y,
                       IMAGE_BOX_W,
                       IMAGE_BOX_H,
                       VGA_IMAGE_BOX_BG);

    vga_print_software_text(10,
                            150,
                            "WAITING FOR VGA IRQ",
                            VGA_WHITE);
}

/* =========================
   Display one captured packet
   Same as old code
   ========================= */

static void display_capture_from_sdram(void)
{
    uint16_t text_len;

    if (!packet_ready())
    {
        printf("IRQ received: no valid packet\n");
        fflush(stdout);

        vga_draw_rectangle(TEXT_REGION_X,
                           TEXT_REGION_Y,
                           TEXT_REGION_W,
                           TEXT_REGION_H,
                           VGA_BLACK);

        vga_print_software_text(TEXT_START_X,
                                TEXT_START_Y,
                                "NO VALID PACKET",
                                VGA_WHITE);
        return;
    }

    text_len = get_text_len();

    vga_draw_rectangle(0, 0, 320, 140, VGA_DARK_BG);

    vga_draw_rectangle(IMAGE_BOX_X,
                       IMAGE_BOX_Y,
                       IMAGE_BOX_W,
                       IMAGE_BOX_H,
                       VGA_IMAGE_BOX_BG);

    draw_image_from_sdram();
    draw_text_from_sdram(text_len);

    printf("Display done\n");
    fflush(stdout);
}

/* =========================
   Debug menu init
   Old static screen
   ========================= */

void debug_init(void)
{
    draw_static_screen();
}

void debug_update(void)
{
    /*
       Nothing here.
       Button IRQ displays image/text.
       Tilt left exit is handled in main.c.
    */
}

/* =========================
   Debug button behaviour
   SAME as old while-loop IRQ handling
   ========================= */

void debug_display_irq_capture_from_sdram(void)
{
    printf("IRQ received\n");
    fflush(stdout);

    /*
       Clear done signal before starting new VGA display update.
    */
    debug_irq_tx_set_false();

    /*
       Display SDRAM capture.
    */
    display_capture_from_sdram();

    /*
       VGA display is done, set VGA_IRQ_TX true.
    */
    vga_irq_tx_set_true();

    /*
       Wait until trigger signal goes low before accepting another.
       Prevents repeated handling while source is still high.
    */
    while ((IORD_ALTERA_AVALON_PIO_DATA(VGA_IRQ_RX_BASE) & VGA_IRQ_RX_ACTIVE_MASK) != 0)
    {
        usleep(1000);
    }

    /*
       Clear any edge that happened while drawing was running.
    */
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(VGA_IRQ_RX_BASE, VGA_IRQ_RX_ACTIVE_MASK);
}
