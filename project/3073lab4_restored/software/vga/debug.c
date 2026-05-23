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
   Debug panel goals
   =========================
   - Text packets are shown live in the bottom log panel.
   - When the log is full, the oldest line is removed and the newest line is added.
   - KEY0 is handled by IMG/Arduino as an image-capture request.
   - When IMG receives the captured image packet, this debug panel displays it.
   - KEY1 copies the currently displayed image into the Draw Pixel background buffer.
   ========================= */

#define VGA_IRQ_RX_BASE       0x9060
#define VGA_IRQ_RX_ACTIVE_MASK 0x01
#define VGA_IRQ_TX_ACTIVE_MASK 0x01

#define TEXT_BUFFER_BASE      0x05200000
#define STATUS_BASE           0x05209000
#define IMAGE_BUFFER_BASE     0x0520A000
#define SHARED_FLAGS_BASE     0x05212000

#define FLAG_IMAGE_READY              0x24
#define FLAG_TEXT_READY_SHARED        0x28
#define FLAG_DEBUG_BG_ACCEPT_SEQ      0x848
#define FLAG_DEBUG_STATUS             0x84C
#define DEBUG_STATUS_IMAGE_READY      2
#define DEBUG_STATUS_BG_ACCEPTED      3
#define DEBUG_STATUS_NO_IMAGE         4

#define DRAW_BG_READY                 0x980
#define DRAW_BG_SEQ                   0x984
#define DRAW_BG_GRID                  0x1000
#define DRAW_GRID_W                   96
#define DRAW_GRID_H                   96
#define DRAW_BG_GRID_SIZE             (DRAW_GRID_W * DRAW_GRID_H)

#define IMAGE_W             96
#define IMAGE_H             96
#define IMAGE_LEN           (IMAGE_W * IMAGE_H)

#define IMAGE_BOX_X         112
#define IMAGE_BOX_Y         16
#define IMAGE_BOX_W         96
#define IMAGE_BOX_H         96

#define TEXT_REGION_X       0
#define TEXT_REGION_Y       140
#define TEXT_REGION_W       320
#define TEXT_REGION_H       100

#define TEXT_START_X        10
#define TEXT_START_Y        150
#define TEXT_LINE_STEP      14
#define DEBUG_MAX_LINES     6
#define DEBUG_LINE_CHARS    8

#define VGA_BLACK           0x00
#define VGA_DARK_BG         0x24
#define VGA_GREEN           0x1C
#define VGA_CYAN            0x1F
#define VGA_RED             0xE0
#define VGA_YELLOW          0xFC
#define VGA_WHITE           0xFF
#define VGA_IMAGE_BOX_BG    0xFF

#define INVERT_IMAGE_GRAYSCALE /* unused for RGB332 payload */ 0

#define STATUS_MAGIC0_OFS       0
#define STATUS_MAGIC1_OFS       1
#define STATUS_TEXT_LEN_OFS     8
#define STATUS_IMAGE_W_OFS      10
#define STATUS_IMAGE_H_OFS      12
#define STATUS_IMAGE_LEN_OFS    16

static char debug_lines[DEBUG_MAX_LINES][DEBUG_LINE_CHARS + 1];
static int debug_line_count = 0;
static uint32_t last_text_seq = 0;
static uint32_t last_image_seq = 0;
static int current_image_valid = 0;

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

static uint32_t shared_read32(uint32_t offset)
{
    return IORD_32DIRECT(SHARED_FLAGS_BASE, offset);
}

static void shared_write32(uint32_t offset, uint32_t value)
{
    IOWR_32DIRECT(SHARED_FLAGS_BASE, offset, value);
}

static void shared_write8(uint32_t offset, uint8_t value)
{
    volatile uint8_t *p = (volatile uint8_t *)(SHARED_FLAGS_BASE + offset);
    *p = value;
}

static void vga_irq_tx_set_true(void)
{
    IOWR_ALTERA_AVALON_PIO_DATA(VGA_IRQ_TX_BASE, VGA_IRQ_TX_ACTIVE_MASK);
}

void debug_irq_tx_set_false(void)
{
    IOWR_ALTERA_AVALON_PIO_DATA(VGA_IRQ_TX_BASE, 0x0);
}

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

static uint32_t get_image_len(void)
{
    volatile uint8_t *status = (volatile uint8_t *)STATUS_BASE;
    return read_u32_le(&status[STATUS_IMAGE_LEN_OFS]);
}

static uint8_t rgb332_to_vga_color(uint8_t rgb332)
{
    /*
       Debug image payload is already one RGB332 byte per pixel:
           bit 7..5 = red, bit 4..2 = green, bit 1..0 = blue.

       Packet size is unchanged from grayscale because both formats are
       1 byte/pixel at 96x96.
    */
    return rgb332;
}

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
            uint8_t rgb332 = img[src_y * IMAGE_W + src_x];
            uint8_t color = rgb332_to_vga_color(rgb332);

            vga_draw_rectangle(IMAGE_BOX_X + x,
                               IMAGE_BOX_Y + y,
                               1,
                               1,
                               color);
        }
    }

    current_image_valid = 1;
    shared_write32(FLAG_DEBUG_STATUS, DEBUG_STATUS_IMAGE_READY);
}

static void draw_image_frame(void)
{
    vga_draw_rectangle(0, 0, 320, 140, VGA_DARK_BG);
    vga_draw_rectangle(IMAGE_BOX_X - 2,
                       IMAGE_BOX_Y - 2,
                       IMAGE_BOX_W + 4,
                       IMAGE_BOX_H + 4,
                       VGA_CYAN);
    vga_draw_rectangle(IMAGE_BOX_X,
                       IMAGE_BOX_Y,
                       IMAGE_BOX_W,
                       IMAGE_BOX_H,
                       VGA_BLACK);
}

static int debug_image_buffer_should_be_valid(void)
{
    if (current_image_valid)
        return 1;

    if (shared_read32(FLAG_IMAGE_READY) != 0)
        return 1;

    if (shared_read32(FLAG_DEBUG_STATUS) == DEBUG_STATUS_IMAGE_READY ||
        shared_read32(FLAG_DEBUG_STATUS) == DEBUG_STATUS_BG_ACCEPTED)
        return 1;

    if (packet_ready() && get_image_len() > 0)
        return 1;

    return 0;
}

static int debug_refresh_image_from_sdram(void)
{
    if (!debug_image_buffer_should_be_valid())
        return 0;

    draw_image_frame();
    draw_image_from_sdram();
    return 1;
}

static void debug_clear_lines(void)
{
    int i;

    for (i = 0; i < DEBUG_MAX_LINES; i++)
        debug_lines[i][0] = '\0';

    debug_line_count = 0;
}

static void debug_add_line(const char *line)
{
    int i;

    if (line == 0 || line[0] == '\0')
        return;

    if (debug_line_count >= DEBUG_MAX_LINES)
    {
        for (i = 1; i < DEBUG_MAX_LINES; i++)
            strcpy(debug_lines[i - 1], debug_lines[i]);
        debug_line_count = DEBUG_MAX_LINES - 1;
    }

    strncpy(debug_lines[debug_line_count], line, DEBUG_LINE_CHARS);
    debug_lines[debug_line_count][DEBUG_LINE_CHARS] = '\0';
    debug_line_count++;
}

static void debug_draw_text_panel(void)
{
    int i;

    vga_draw_rectangle(TEXT_REGION_X,
                       TEXT_REGION_Y,
                       TEXT_REGION_W,
                       TEXT_REGION_H,
                       VGA_BLACK);

    if (debug_line_count == 0)
    {
        vga_print_software_text(TEXT_START_X,
                                TEXT_START_Y,
                                "WAITING FOR REALTIME TEXT",
                                VGA_WHITE);
        return;
    }

    for (i = 0; i < debug_line_count; i++)
    {
        vga_print_software_text(TEXT_START_X,
                                TEXT_START_Y + i * TEXT_LINE_STEP,
                                debug_lines[i],
                                VGA_WHITE);
    }
}

static void debug_append_text_from_sdram(uint16_t text_len)
{
    char line[DEBUG_LINE_CHARS + 1];
    int line_pos = 0;
    uint16_t i;

    if (text_len == 0)
        return;

    if (text_len > 512)
        text_len = 512;

    memset(line, 0, sizeof(line));

    for (i = 0; i < text_len; i++)
    {
        char c = (char)(IORD_8DIRECT(TEXT_BUFFER_BASE, i) & 0xFF);

        /*
           Debug text now preserves row boundaries from Arduino.
           Newline means "draw next VGA debug line", not a normal space.
        */
        if (c == '\0')
            break;

        if (c == '\r' || c == '\n')
        {
            if (line_pos > 0)
            {
                line[line_pos] = '\0';
                debug_add_line(line);
                line_pos = 0;
                memset(line, 0, sizeof(line));
            }
            continue;
        }

        if (c == '\t')
            c = ' ';
        if (c < 32 || c > 126)
            c = ' ';

        line[line_pos++] = c;

        if (line_pos >= DEBUG_LINE_CHARS)
        {
            line[line_pos] = '\0';
            debug_add_line(line);
            line_pos = 0;
            memset(line, 0, sizeof(line));
        }
    }

    if (line_pos > 0)
    {
        line[line_pos] = '\0';
        debug_add_line(line);
    }

    debug_draw_text_panel();
}

static void draw_static_screen(void)
{
    draw_image_frame();
    vga_draw_rectangle(0, 140, 320, 100, VGA_BLACK);

    /* Keep the top-left text tiny so it does not cover the image preview. */
    vga_print_software_text(4, 118, "K0 IMG  K1 BG  SW9 MENU", VGA_YELLOW);

    if (!debug_refresh_image_from_sdram())
    {
        vga_print_software_text(136, 60, "NO IMG", VGA_WHITE);
    }

    debug_draw_text_panel();
}

static void display_capture_from_sdram(void)
{
    uint16_t text_len;
    uint32_t image_len;

    if (!packet_ready())
    {
        printf("Debug display: no valid packet\n");
        fflush(stdout);
        debug_add_line("NO VALID PACKET");
        debug_draw_text_panel();
        return;
    }

    text_len = get_text_len();
    image_len = get_image_len();

    if (image_len > 0)
    {
        debug_refresh_image_from_sdram();
    }

    if (text_len > 0)
        debug_append_text_from_sdram(text_len);

    printf("Debug display update: text_len=%u image_len=%lu\n",
           (unsigned int)text_len,
           (unsigned long)image_len);
    fflush(stdout);
}

void debug_init(void)
{
    debug_clear_lines();
    last_text_seq = shared_read32(FLAG_TEXT_READY_SHARED);
    last_image_seq = shared_read32(FLAG_IMAGE_READY);
    current_image_valid = debug_image_buffer_should_be_valid();

    draw_static_screen();
}

void debug_update(void)
{
    uint32_t text_seq;
    uint32_t image_seq;

    text_seq = shared_read32(FLAG_TEXT_READY_SHARED);
    image_seq = shared_read32(FLAG_IMAGE_READY);

    if (image_seq != last_image_seq)
    {
        last_image_seq = image_seq;

        if (debug_refresh_image_from_sdram())
        {
            debug_add_line("IMAGE CAPTURED");
            debug_draw_text_panel();
        }
    }

    if (text_seq != last_text_seq)
    {
        last_text_seq = text_seq;

        if (packet_ready())
            debug_append_text_from_sdram(get_text_len());
    }
}

void debug_note_image_capture_requested(void)
{
    debug_add_line("KEY0 IMAGE CAPTURE REQ");
    debug_draw_text_panel();
}

void debug_accept_current_image_as_draw_background(void)
{
    volatile uint8_t *img = (volatile uint8_t *)IMAGE_BUFFER_BASE;
    int x;
    int y;
    uint32_t seq;

    if (!current_image_valid)
    {
        debug_refresh_image_from_sdram();
    }

    if (!debug_image_buffer_should_be_valid())
    {
        debug_add_line("NO IMAGE TO USE AS BG");
        debug_draw_text_panel();
        shared_write32(FLAG_DEBUG_STATUS, DEBUG_STATUS_NO_IMAGE);
        return;
    }

    for (y = 0; y < DRAW_GRID_H; y++)
    {
        int src_y = (y * IMAGE_H) / DRAW_GRID_H;

        for (x = 0; x < DRAW_GRID_W; x++)
        {
            int src_x = (x * IMAGE_W) / DRAW_GRID_W;
            uint8_t rgb332 = img[src_y * IMAGE_W + src_x];
            uint8_t color = rgb332_to_vga_color(rgb332);
            shared_write8(DRAW_BG_GRID + y * DRAW_GRID_W + x, color);
        }
    }

    seq = shared_read32(DRAW_BG_SEQ) + 1;
    shared_write32(DRAW_BG_SEQ, seq);
    shared_write32(DRAW_BG_READY, 1);
    shared_write32(FLAG_DEBUG_BG_ACCEPT_SEQ, shared_read32(FLAG_DEBUG_BG_ACCEPT_SEQ) + 1);
    shared_write32(FLAG_DEBUG_STATUS, DEBUG_STATUS_BG_ACCEPTED);

    debug_add_line("IMAGE SAVED AS 96X96 DRAW BG");
    debug_draw_text_panel();

    printf("Debug image accepted as draw background, seq=%lu\n", (unsigned long)seq);
    fflush(stdout);
}

void debug_display_irq_capture_from_sdram(void)
{
    printf("Debug manual display requested\n");
    fflush(stdout);

    debug_irq_tx_set_false();
    display_capture_from_sdram();
    vga_irq_tx_set_true();

    while ((IORD_ALTERA_AVALON_PIO_DATA(VGA_IRQ_RX_BASE) & VGA_IRQ_RX_ACTIVE_MASK) != 0)
        usleep(1000);

    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(VGA_IRQ_RX_BASE, VGA_IRQ_RX_ACTIVE_MASK);
}
