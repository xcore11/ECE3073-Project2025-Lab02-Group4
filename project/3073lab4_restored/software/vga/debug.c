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

#define TEXT_START_X        0
#define TEXT_START_Y        150
#define TEXT_LINE_STEP      14
#define DEBUG_MAX_LINES     6
#define DEBUG_FONT_CHAR_W   8
#define DEBUG_RIGHT_MARGIN  0
#define DEBUG_WIRE_ROW_CHARS 8  /* Arduino DEBUG OCR row width; keep short packets aligned. */
#define DEBUG_VISIBLE_LINE_CHARS ((TEXT_REGION_X + TEXT_REGION_W - TEXT_START_X - DEBUG_RIGHT_MARGIN) / DEBUG_FONT_CHAR_W)
#define DEBUG_LINE_CHARS    ((DEBUG_VISIBLE_LINE_CHARS / DEBUG_WIRE_ROW_CHARS) * DEBUG_WIRE_ROW_CHARS)

#define VGA_BLACK           0x00
#define VGA_DARK_BG         0x24
#define VGA_GREEN           0x1C
#define VGA_CYAN            0x1F
#define VGA_RED             0xE0
#define VGA_YELLOW          0xFC
#define VGA_WHITE           0xFF
#define VGA_IMAGE_BOX_BG    0xFF

#define INVERT_IMAGE_GRAYSCALE /* unused for RGB332 payload */ 0

#define DEBUG_TEXT_CONSOLE_PRINT 1
#define DEBUG_SUPPRESS_SINGLE_CHAR_FRAGMENTS 1

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
static int debug_last_line_is_text_fragment = 0;

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

#if DEBUG_TEXT_CONSOLE_PRINT
static void debug_console_print_escaped_char(char c)
{
    unsigned char uc = (unsigned char)c;

    if (c == '\r')
    {
        printf("\\r");
    }
    else if (c == '\n')
    {
        printf("\\n");
    }
    else if (c == '\t')
    {
        printf("\\t");
    }
    else if (uc >= 32 && uc <= 126)
    {
        putchar(c);
    }
    else
    {
        printf("\\x%02X", (unsigned int)uc);
    }
}

static void debug_console_print_text_packet(uint16_t text_len)
{
    uint16_t i;
    uint16_t capped_len = text_len;

    if (capped_len > 512)
        capped_len = 512;

    printf("[VGA DEBUG RAW TEXT] text_len=%u capped=%u raw=\"",
           (unsigned int)text_len,
           (unsigned int)capped_len);

    for (i = 0; i < capped_len; i++)
    {
        char c = (char)(IORD_8DIRECT(TEXT_BUFFER_BASE, i) & 0xFF);
        debug_console_print_escaped_char(c);
    }

    printf("\"\n");
    fflush(stdout);
}

static void debug_console_print_lines(const char *reason)
{
    int i;

    printf("[VGA DEBUG DISPLAY BUFFER] reason=%s count=%d line_width=%d\n",
           reason,
           debug_line_count,
           DEBUG_LINE_CHARS);

    for (i = 0; i < debug_line_count; i++)
    {
        printf("  line[%d] len=%u text=\"%s\"\n",
               i,
               (unsigned int)strlen(debug_lines[i]),
               debug_lines[i]);
    }

    fflush(stdout);
}
#else
#define debug_console_print_text_packet(text_len) do { (void)(text_len); } while (0)
#define debug_console_print_lines(reason) do { (void)(reason); } while (0)
#endif

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
    debug_last_line_is_text_fragment = 0;
}

static void debug_make_empty_line_for_append(void)
{
    int i;

    if (debug_line_count >= DEBUG_MAX_LINES)
    {
        for (i = 1; i < DEBUG_MAX_LINES; i++)
            strcpy(debug_lines[i - 1], debug_lines[i]);
        debug_line_count = DEBUG_MAX_LINES - 1;
    }

    debug_lines[debug_line_count][0] = '\0';
    debug_line_count++;
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

#if DEBUG_TEXT_CONSOLE_PRINT
    printf("[VGA DEBUG ADD LINE] text=\"%s\"\n", debug_lines[debug_line_count - 1]);
    fflush(stdout);
#endif

    /* System/status lines should not be merged with the next OCR fragment. */
    debug_last_line_is_text_fragment = 0;
}

static void debug_append_text_fragment(const char *fragment)
{
    int i;
    int fragment_len;

    if (fragment == 0 || fragment[0] == '\0')
        return;

    fragment_len = (int)strlen(fragment);

    /*
       Normal realtime DEBUG packets arrive as fixed 8-character OCR slices.
       The old code appended them one byte at a time into a 38-character VGA
       line. Because 38 is not divisible by 8, the last one or two characters
       of a slice could wrap onto the next VGA row and appear as lone letters
       such as "Z" or lone numbers such as "5".

       Keep one wire slice together whenever possible.  DEBUG_LINE_CHARS is
       also forced to a multiple of DEBUG_WIRE_ROW_CHARS, so five 8-character
       slices fit exactly across the 320-pixel text panel.
    */
    if (fragment_len > 0 && fragment_len <= DEBUG_WIRE_ROW_CHARS)
    {
        int last_index;
        int len;

        if (debug_line_count == 0 || !debug_last_line_is_text_fragment)
        {
            debug_make_empty_line_for_append();
            debug_last_line_is_text_fragment = 1;
        }

        last_index = debug_line_count - 1;
        len = (int)strlen(debug_lines[last_index]);

        if (len > 0 && (len + fragment_len) > DEBUG_LINE_CHARS)
        {
            debug_make_empty_line_for_append();
            debug_last_line_is_text_fragment = 1;
            last_index = debug_line_count - 1;
            len = 0;
        }

        for (i = 0; i < fragment_len && len < DEBUG_LINE_CHARS; i++)
        {
            debug_lines[last_index][len++] = fragment[i];
        }
        debug_lines[last_index][len] = '\0';
        return;
    }

    i = 0;
    while (fragment[i] != '\0')
    {
        int last_index;
        int len;

        if (debug_line_count == 0 ||
            !debug_last_line_is_text_fragment ||
            strlen(debug_lines[debug_line_count - 1]) >= DEBUG_LINE_CHARS)
        {
            debug_make_empty_line_for_append();
            debug_last_line_is_text_fragment = 1;
        }

        last_index = debug_line_count - 1;
        len = (int)strlen(debug_lines[last_index]);

        if (len < DEBUG_LINE_CHARS)
        {
            debug_lines[last_index][len] = fragment[i];
            debug_lines[last_index][len + 1] = '\0';
            i++;
        }
    }
}

static void debug_end_current_text_instruction(void)
{
    /*
       A real newline from IMG/Arduino means the next OCR text belongs to a
       new debug instruction.  This keeps instruction boundaries, while still
       allowing short fragments from separate packets to fill the full VGA box.
    */
    debug_last_line_is_text_fragment = 0;
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

static int debug_packet_contains_newline(uint16_t text_len)
{
    uint16_t i;

    if (text_len > 512)
        text_len = 512;

    for (i = 0; i < text_len; i++)
    {
        char c = (char)(IORD_8DIRECT(TEXT_BUFFER_BASE, i) & 0xFF);
        if (c == '\n' || c == '\r')
            return 1;
    }

    return 0;
}

static uint16_t debug_effective_text_len(uint16_t text_len)
{
    uint16_t i;
    uint16_t last_meaningful = 0;

    if (text_len > 512)
        text_len = 512;

    for (i = 0; i < text_len; i++)
    {
        char c = (char)(IORD_8DIRECT(TEXT_BUFFER_BASE, i) & 0xFF);

        if (c == '\0')
            break;

        if (c == '\r' || c == '\n')
        {
            last_meaningful = (uint16_t)(i + 1);
            continue;
        }

        if (c != ' ' && c != '\t')
            last_meaningful = (uint16_t)(i + 1);
    }

    return last_meaningful;
}

static void debug_append_text_from_sdram(uint16_t text_len)
{
    char fragment[DEBUG_LINE_CHARS + 1];
    int fragment_pos = 0;
    uint16_t i;
    uint16_t effective_len;
    uint16_t read_len;
    int has_newline;

    if (text_len == 0)
        return;

    if (text_len > 512)
        text_len = 512;

    debug_console_print_text_packet(text_len);

    has_newline = debug_packet_contains_newline(text_len);

    /*
       The VGA log proved the bad display was not caused by the VGA wrapping a
       complete row incorrectly.  VGA was sometimes receiving real one-byte
       OCR fragments such as "d", "h", "z", "r" and then the old debug code
       padded each one to an 8-column slot.  That is why the screen showed
       orphan first characters like Z / 5 at the beginning of lines.

       Keep the normal 8-column padding for useful 2..7 byte OCR slices, but
       suppress single-character non-newline fragments because they are almost
       always unstable partial OCR / stale edge packets, not useful commands.
    */
    effective_len = debug_effective_text_len(text_len);
    read_len = effective_len;

#if DEBUG_SUPPRESS_SINGLE_CHAR_FRAGMENTS
    if (!has_newline && effective_len <= 1)
    {
#if DEBUG_TEXT_CONSOLE_PRINT
        printf("[VGA DEBUG SUPPRESS] ignored one-character fragment, raw_len=%u effective_len=%u\n",
               (unsigned int)text_len,
               (unsigned int)effective_len);
        fflush(stdout);
#endif
        return;
    }
#endif

    if (!has_newline && effective_len > 1 && effective_len < DEBUG_WIRE_ROW_CHARS)
        effective_len = DEBUG_WIRE_ROW_CHARS;

    memset(fragment, 0, sizeof(fragment));

    for (i = 0; i < effective_len; i++)
    {
        char c;

        if (i < read_len)
            c = (char)(IORD_8DIRECT(TEXT_BUFFER_BASE, i) & 0xFF);
        else
            c = ' ';

        if (i < read_len && c == '\0')
            break;

        if (c == '\r' || c == '\n')
        {
            if (fragment_pos > 0)
            {
                fragment[fragment_pos] = '\0';
                debug_append_text_fragment(fragment);
                fragment_pos = 0;
                memset(fragment, 0, sizeof(fragment));
            }

            debug_end_current_text_instruction();
            continue;
        }

        if (c == '\t')
            c = ' ';
        if (c < 32 || c > 126)
            c = ' ';

        fragment[fragment_pos++] = c;

        if (fragment_pos >= DEBUG_LINE_CHARS)
        {
            fragment[fragment_pos] = '\0';
            debug_append_text_fragment(fragment);
            fragment_pos = 0;
            memset(fragment, 0, sizeof(fragment));
        }
    }

    if (fragment_pos > 0)
    {
        fragment[fragment_pos] = '\0';
        debug_append_text_fragment(fragment);
    }

    debug_draw_text_panel();
    debug_console_print_lines("after_text_packet");
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
#if DEBUG_TEXT_CONSOLE_PRINT
        printf("[VGA DEBUG TEXT SEQ] old=%lu new=%lu packet_ready=%d text_len=%u\n",
               (unsigned long)last_text_seq,
               (unsigned long)text_seq,
               packet_ready(),
               packet_ready() ? (unsigned int)get_text_len() : 0u);
        fflush(stdout);
#endif

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
