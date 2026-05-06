#include "io.h"
#include "system.h"
#include "vga.h"

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "altera_avalon_pio_regs.h"
#include "sys/alt_irq.h"

/* =========================
   VGA IRQ RX defines

   If system.h already gives you these automatically,
   you can remove these manual defines.
   ========================= */

#define VGA_IRQ_IRQ_RX 1
#define VGA_IRQ_IRQ_RX_INTERRUPT_CONTROLLER_ID 0
#define VGA_IRQ_RX_BASE 0x9060

extern void vga_init(void);

/* =========================
   VGA IRQ TX
   This must exist in system.h as an output PIO.
   Example:
   #define VGA_IRQ_TX_BASE ...
   ========================= */

#define VGA_IRQ_RX_ACTIVE_MASK 0x01
#define VGA_IRQ_TX_ACTIVE_MASK 0x01

/* =========================
   SDRAM locations written by image processor
   ========================= */

#define TEXT_BUFFER_BASE    0x05200000
#define STATUS_BASE         0x05209000
#define IMAGE_BUFFER_BASE   0x0520A000

/* =========================
   Image format from ESP
   ========================= */

#define IMAGE_W             96
#define IMAGE_H             96
#define IMAGE_LEN           (IMAGE_W * IMAGE_H)

/* =========================
   VGA layout
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
   ========================= */

#define VGA_BLACK           0x0
#define VGA_WHITE           0xF
#define VGA_DARK_BG         0x1
#define VGA_IMAGE_BOX_BG    0xF

#define INVERT_IMAGE_GRAYSCALE 0

static volatile unsigned int vga_trigger_count = 0;
static volatile int vga_irq_pending = 0;

/* =========================
   Packet header offsets in STATUS_BASE
   ========================= */

#define STATUS_MAGIC0_OFS       0
#define STATUS_MAGIC1_OFS       1
#define STATUS_VERSION_OFS      2
#define STATUS_STATUS_OFS       3
#define STATUS_TOTAL_LEN_OFS    4
#define STATUS_TEXT_LEN_OFS     8
#define STATUS_IMAGE_W_OFS      10
#define STATUS_IMAGE_H_OFS      12
#define STATUS_LINE_COUNT_OFS   14
#define STATUS_IMAGE_LEN_OFS    16
#define STATUS_HEADER_LEN_OFS   20
#define STATUS_FLAGS_OFS        22

/* =========================
   Little endian readers
   ========================= */

static uint16_t read_u16_le(volatile uint8_t *p)
{
    return ((uint16_t)p[0]) | ((uint16_t)p[1] << 8);
}

static uint32_t read_u32_le(volatile uint8_t *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/* =========================
   VGA IRQ TX helpers
   ========================= */

static void vga_irq_tx_set_true(void)
{
    /*
       Tell the other side VGA display is done.
    */
    IOWR_ALTERA_AVALON_PIO_DATA(VGA_IRQ_TX_BASE, VGA_IRQ_TX_ACTIVE_MASK);
}

static void vga_irq_tx_set_false(void)
{
    /*
       Reset VGA done signal back to low.
    */
    IOWR_ALTERA_AVALON_PIO_DATA(VGA_IRQ_TX_BASE, 0x0);
}

/* =========================
   Status helpers
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

static uint32_t get_total_len(void)
{
    volatile uint8_t *status = (volatile uint8_t *)STATUS_BASE;
    return read_u32_le(&status[STATUS_TOTAL_LEN_OFS]);
}

static uint16_t get_line_count(void)
{
    volatile uint8_t *status = (volatile uint8_t *)STATUS_BASE;
    return read_u16_le(&status[STATUS_LINE_COUNT_OFS]);
}

/* =========================
   8-bit grayscale to 4-bit VGA grayscale
   ========================= */

static uint8_t gray_to_vga_color(uint8_t g)
{
#if INVERT_IMAGE_GRAYSCALE
    g = 255 - g;
#endif

    /*
       ESP image:
       0   = black
       255 = white

       VGA pixel:
       0x0 = black
       0xF = white
    */
    return (g >> 4) & 0x0F;
}

/* =========================
   Debug status print
   ========================= */

static void print_status_from_sdram(void)
{
    volatile uint8_t *s = (volatile uint8_t *)STATUS_BASE;
    volatile char *text = (volatile char *)TEXT_BUFFER_BASE;
    volatile uint8_t *img = (volatile uint8_t *)IMAGE_BUFFER_BASE;

    uint8_t magic0 = s[STATUS_MAGIC0_OFS];
    uint8_t magic1 = s[STATUS_MAGIC1_OFS];

    uint32_t total_len = read_u32_le(&s[STATUS_TOTAL_LEN_OFS]);
    uint16_t text_len = read_u16_le(&s[STATUS_TEXT_LEN_OFS]);
    uint16_t image_w = read_u16_le(&s[STATUS_IMAGE_W_OFS]);
    uint16_t image_h = read_u16_le(&s[STATUS_IMAGE_H_OFS]);
    uint16_t line_count = read_u16_le(&s[STATUS_LINE_COUNT_OFS]);
    uint32_t image_len = read_u32_le(&s[STATUS_IMAGE_LEN_OFS]);
    uint16_t flags = read_u16_le(&s[STATUS_FLAGS_OFS]);

    int i;

    printf("\n========== VGA SDRAM STATUS CHECK ==========\n");
    printf("STATUS_BASE       = 0x%08X\n", STATUS_BASE);
    printf("TEXT_BUFFER_BASE  = 0x%08X\n", TEXT_BUFFER_BASE);
    printf("IMAGE_BUFFER_BASE = 0x%08X\n", IMAGE_BUFFER_BASE);

    printf("magic             = 0x%02X 0x%02X [%c%c]\n",
           magic0,
           magic1,
           (magic0 >= 32 && magic0 <= 126) ? magic0 : '.',
           (magic1 >= 32 && magic1 <= 126) ? magic1 : '.');

    printf("total_len         = %lu\n", (unsigned long)total_len);
    printf("text_len          = %u\n", text_len);
    printf("image_w           = %u\n", image_w);
    printf("image_h           = %u\n", image_h);
    printf("line_count        = %u\n", line_count);
    printf("image_len         = %lu\n", (unsigned long)image_len);
    printf("flags             = 0x%04X\n", flags);

    printf("text preview      = [");
    for (i = 0; i < text_len && i < 80; i++)
    {
        char c = text[i];

        if (c == '\0')
        {
            break;
        }

        putchar(c);
    }
    printf("]\n");

    printf("image first 20    = ");
    for (i = 0; i < 20; i++)
    {
        printf("%02X ", img[i]);
    }
    printf("\n");

    printf("image last 20     = ");
    for (i = IMAGE_LEN - 20; i < IMAGE_LEN; i++)
    {
        printf("%02X ", img[i]);
    }
    printf("\n");

    printf("packet_ready      = %s\n", packet_ready() ? "YES" : "NO");
    printf("===========================================\n");
    fflush(stdout);
}

/* =========================
   Draw image from SDRAM
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
   ========================= */

static void draw_text_from_sdram(uint16_t text_len)
{
    volatile char *text = (volatile char *)TEXT_BUFFER_BASE;

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
        vga_print_software_text(TEXT_START_X,
                                TEXT_START_Y,
                                "NO TEXT",
                                VGA_WHITE);
        return;
    }

    memset(line, 0, sizeof(line));

    for (i = 0; i < text_len; i++)
    {
        char c = text[i];

        if (c == '\0')
        {
            break;
        }

        line[line_pos++] = c;

        if (line_pos >= max_chars || i == text_len - 1)
        {
            line[line_pos] = '\0';

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
}

/* =========================
   Draw static screen
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
   ========================= */

static void display_capture_from_sdram(void)
{
    uint16_t text_len;
    uint32_t total_len;
    uint16_t line_count;

    if (!packet_ready())
    {
        printf("VGA IRQ received, but packet is not ready in SDRAM\n");
        print_status_from_sdram();

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
    total_len = get_total_len();
    line_count = get_line_count();

    printf("VGA IRQ received, displaying capture\n");
    printf("VGA: total_len=%lu text_len=%u line_count=%u\n",
           (unsigned long)total_len,
           text_len,
           line_count);
    fflush(stdout);

    /*
       Redraw image region and box once per interrupt.
    */
    vga_draw_rectangle(0, 0, 320, 140, VGA_DARK_BG);

    vga_draw_rectangle(IMAGE_BOX_X,
                       IMAGE_BOX_Y,
                       IMAGE_BOX_W,
                       IMAGE_BOX_H,
                       VGA_IMAGE_BOX_BG);

    draw_image_from_sdram();
    draw_text_from_sdram(text_len);

    print_status_from_sdram();
}

/* =========================
   VGA RX IRQ ISR
   ========================= */

static void vga_irq_rx_isr(void* context)
{
    /*
       Clear/acknowledge PIO edge interrupt.
    */
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(VGA_IRQ_RX_BASE, VGA_IRQ_RX_ACTIVE_MASK);

    /*
       Tell main loop to do the slow drawing work.
    */
    vga_irq_pending = 1;
}

/* =========================
   VGA RX IRQ setup
   ========================= */

static void vga_irq_rx_setup(void)
{
    /*
       Disable first while configuring.
    */
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(VGA_IRQ_RX_BASE, 0x0);

    /*
       Clear pending edge capture.
    */
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(VGA_IRQ_RX_BASE, VGA_IRQ_RX_ACTIVE_MASK);

    /*
       Register ISR.
    */
    alt_irq_register(
        VGA_IRQ_IRQ_RX,
        NULL,
        vga_irq_rx_isr
    );

    /*
       Clear again after registering, then enable.
    */
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(VGA_IRQ_RX_BASE, VGA_IRQ_RX_ACTIVE_MASK);
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(VGA_IRQ_RX_BASE, VGA_IRQ_RX_ACTIVE_MASK);
}

/* =========================
   main
   ========================= */

int main(void)
{
    vga_init();

    printf("VGA display processor started\n");
    printf("Setting up VGA_irq_rx interrupt...\n");
    printf("Waiting for interrupt before displaying SDRAM capture\n");
    printf("Image grayscale invert = %d\n", INVERT_IMAGE_GRAYSCALE);
    fflush(stdout);

    /*
       Make sure VGA_IRQ_TX starts low.
    */
    vga_irq_tx_set_false();

    draw_static_screen();

    vga_irq_rx_setup();

    printf("Ready. Waiting for VGA_irq_rx interrupt...\n");
    fflush(stdout);

    while (1)
    {
        if (vga_irq_pending)
        {
            vga_irq_pending = 0;
            vga_trigger_count++;

            printf("\ninterrupted, vga_trigger_count=%u\n", vga_trigger_count);
            fflush(stdout);

            /*
               Clear done signal before starting new VGA display update.
            */
            vga_irq_tx_set_false();

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
    }

    return 0;
}
