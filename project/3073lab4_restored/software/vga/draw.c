/*
 * time.c compatibility build unit
 *
 * Weak draw_game_init() / draw_game_update() fallback.
 *
 * Current behavior:
 *   - DRAW panel resolution is 96x96
 *   - draw colors are full RGB332 bytes
 *   - debug KEY1 background handoff copies a 96x96 RGB332 image 1:1
 *   - RX status dot:
 *       red   = idle
 *       green = receiving realtime rows
 */

#include "vga.h"
#include "pixel_theme.h"
#include "io.h"

#include <stdint.h>
#include <stdio.h>

#define SHARED_FLAGS_BASE              0x05212000

#define FLAG_RT_ACTIVITY_SEQ           0x870
#define FLAG_RT_PANEL_MODE             0x874
#define GAME_MODE_DRAW                 2

#define DEBUG_CONTROL_BASE             0x06000000
#define FLAG_CONTROL_EVENT_SEQ         0x800
#define FLAG_CONTROL_LAST_EVENT_TYPE   0x814
#define FLAG_CONTROL_LAST_EVENT_VALUE  0x818
#define FLAG_CONTROL_MESSAGE           0x828
#define DEBUG_CONTROL_MESSAGE_BYTES    68
#define DEBUG_CONTROL_CMD_BATCH        100
#define DEBUG_CONTROL_MASK_HEX_MESSAGE 0x00000001u

#define RX_IDLE_COLOR                  0xE0
#define RX_ACTIVE_COLOR                0x1C
#define RX_ACTIVE_TICKS                30

#define DRAW_BG_READY                  0x980
#define DRAW_BG_SEQ                    0x984
#define DRAW_BG_GRID                   0x1000

#define DRAW_MB_READY                  0x900
#define DRAW_MB_ACK                    0x904
#define DRAW_MB_SEQ                    0x908
#define DRAW_MB_X                      0x90C
#define DRAW_MB_Y                      0x910
#define DRAW_MB_COLOR                  0x914
#define DRAW_MB_STATUS                 0x918

#define FLAG_DRAW_RED_COUNT            0xCC0
#define FLAG_DRAW_GREEN_COUNT          0xCC4
#define FLAG_DRAW_BLUE_COUNT           0xCC8
#define FLAG_DRAW_YELLOW_COUNT         0xCCC
#define FLAG_DRAW_BLACK_COUNT          0xCD0
#define FLAG_DRAW_WHITE_COUNT          0xCD4
#define FLAG_DRAW_OTHER_COUNT          0xCD8
#define FLAG_SFX_CLEAR                  0xC6C

#define DRAW_STATUS_OK                 0
#define DRAW_STATUS_BAD_COORD          1
#define DRAW_STATUS_BAD_COLOR          2

#define DRAW_GRID_X0                   64
#define DRAW_GRID_Y0                   20
#define DRAW_GRID_W                    96
#define DRAW_GRID_H                    96
#define DRAW_CELL_SIZE                 2

#ifndef COL_BLACK
#define COL_BLACK                      0x00
#endif
#ifndef COL_GREEN
#define COL_GREEN                      0x1C
#endif
#ifndef COL_CYAN
#define COL_CYAN                       0x1F
#endif
#ifndef COL_YELLOW
#define COL_YELLOW                     0xFC
#endif
#ifndef COL_RED
#define COL_RED                        0xE0
#endif
#ifndef COL_BLUE
#define COL_BLUE                       0x03
#endif
#ifndef COL_WHITE
#define COL_WHITE                      0xFF
#endif

static uint32_t time_draw_last_seq = 0;
static uint32_t time_draw_last_bg_seq = 0;
static uint32_t time_draw_rx_last_seq = 0;
static int time_draw_rx_ticks = 0;
static int time_draw_rx_indicator_state = -1;
static uint8_t time_draw_grid[DRAW_GRID_W * DRAW_GRID_H];
static uint32_t draw_count_red = 0;
static uint32_t draw_count_green = 0;
static uint32_t draw_count_blue = 0;
static uint32_t draw_count_yellow = 0;
static uint32_t draw_count_black = 0;
static uint32_t draw_count_white = 0;
static uint32_t draw_count_other = 0;

static uint32_t time_shared_read32(uint32_t offset)
{
    return IORD_32DIRECT(SHARED_FLAGS_BASE, offset);
}

static void time_shared_write32(uint32_t offset, uint32_t value)
{
    IOWR_32DIRECT(SHARED_FLAGS_BASE, offset, value);
}

static void trigger_sfx_flag(uint32_t offset)
{
    time_shared_write32(offset, time_shared_read32(offset) + 1);
}

static void debug_write32(uint32_t offset, uint32_t value)
{
    IOWR_32DIRECT(DEBUG_CONTROL_BASE, offset, value);
}

static uint32_t debug_read32(uint32_t offset)
{
    return IORD_32DIRECT(DEBUG_CONTROL_BASE, offset);
}

static void debug_write_message(const char *msg)
{
    int i;
    for (i = 0; i < DEBUG_CONTROL_MESSAGE_BYTES; i++)
        IOWR_8DIRECT(DEBUG_CONTROL_BASE, FLAG_CONTROL_MESSAGE + i, 0);
    if (msg == 0)
        return;
    for (i = 0; i < DEBUG_CONTROL_MESSAGE_BYTES - 1 && msg[i] != '\0'; i++)
        IOWR_8DIRECT(DEBUG_CONTROL_BASE, FLAG_CONTROL_MESSAGE + i, (uint8_t)msg[i]);
}

static void publish_control_message(const char *msg)
{
    uint32_t seq;
    debug_write_message(msg);
    debug_write32(FLAG_CONTROL_LAST_EVENT_TYPE, DEBUG_CONTROL_CMD_BATCH);
    debug_write32(FLAG_CONTROL_LAST_EVENT_VALUE, DEBUG_CONTROL_MASK_HEX_MESSAGE);
    seq = debug_read32(FLAG_CONTROL_EVENT_SEQ) + 1;
    debug_write32(FLAG_CONTROL_EVENT_SEQ, seq);
}

static void count_color_add(uint8_t color, int delta)
{
    uint32_t *target = &draw_count_other;

    if (color == COL_RED) target = &draw_count_red;
    else if (color == COL_GREEN) target = &draw_count_green;
    else if (color == COL_BLUE) target = &draw_count_blue;
    else if (color == COL_YELLOW) target = &draw_count_yellow;
    else if (color == COL_BLACK) target = &draw_count_black;
    else if (color == COL_WHITE) target = &draw_count_white;

    if (delta < 0) {
        if (*target > 0) (*target)--;
    } else {
        (*target)++;
    }
}

static void publish_draw_counts(void)
{
    char msg[DEBUG_CONTROL_MESSAGE_BYTES];
    time_shared_write32(FLAG_DRAW_RED_COUNT, draw_count_red);
    time_shared_write32(FLAG_DRAW_GREEN_COUNT, draw_count_green);
    time_shared_write32(FLAG_DRAW_BLUE_COUNT, draw_count_blue);
    time_shared_write32(FLAG_DRAW_YELLOW_COUNT, draw_count_yellow);
    time_shared_write32(FLAG_DRAW_BLACK_COUNT, draw_count_black);
    time_shared_write32(FLAG_DRAW_WHITE_COUNT, draw_count_white);
    time_shared_write32(FLAG_DRAW_OTHER_COUNT, draw_count_other);

    snprintf(msg, sizeof(msg), "R%lu G%lu B%lu Y%lu K%lu W%lu O%lu",
             (unsigned long)draw_count_red,
             (unsigned long)draw_count_green,
             (unsigned long)draw_count_blue,
             (unsigned long)draw_count_yellow,
             (unsigned long)draw_count_black,
             (unsigned long)draw_count_white,
             (unsigned long)draw_count_other);
    publish_control_message(msg);
}

static void recount_draw_colors(void)
{
    int i;
    draw_count_red = draw_count_green = draw_count_blue = draw_count_yellow = 0;
    draw_count_black = draw_count_white = draw_count_other = 0;
    for (i = 0; i < DRAW_GRID_W * DRAW_GRID_H; i++)
        count_color_add(time_draw_grid[i], 1);
    publish_draw_counts();
}

static uint8_t time_shared_read8(uint32_t offset)
{
    volatile uint8_t *p = (volatile uint8_t *)(SHARED_FLAGS_BASE + offset);
    return *p;
}

static int time_draw_valid_cell(int x, int y)
{
    return (x >= 0 && x < DRAW_GRID_W && y >= 0 && y < DRAW_GRID_H);
}

static int time_draw_grid_index(int x, int y)
{
    return y * DRAW_GRID_W + x;
}

static void time_draw_cell(int x, int y, uint8_t color)
{
    int px;
    int py;

    if (!time_draw_valid_cell(x, y))
        return;

    px = DRAW_GRID_X0 + x * DRAW_CELL_SIZE;
    py = DRAW_GRID_Y0 + y * DRAW_CELL_SIZE;

    pt_draw_canvas_cell(px, py, DRAW_CELL_SIZE, color);
}

static void time_draw_rx_indicator(int active)
{
    pt_draw_rx_badge(10, 7, active, active ? "RX" : "ID");
}

static void time_draw_update_rx_status(void)
{
    uint32_t seq = time_shared_read32(FLAG_RT_ACTIVITY_SEQ);
    uint32_t panel = time_shared_read32(FLAG_RT_PANEL_MODE);
    int active;

    if (panel == GAME_MODE_DRAW && seq != time_draw_rx_last_seq)
    {
        time_draw_rx_last_seq = seq;
        time_draw_rx_ticks = RX_ACTIVE_TICKS;
    }
    else if (time_draw_rx_ticks > 0)
    {
        time_draw_rx_ticks--;
    }

    active = (time_draw_rx_ticks > 0);

    if (active != time_draw_rx_indicator_state)
    {
        time_draw_rx_indicator_state = active;
        time_draw_rx_indicator(active);
    }
}

static void time_draw_border(void)
{
    int x = DRAW_GRID_X0 - 2;
    int y = DRAW_GRID_Y0 - 2;
    int w = DRAW_GRID_W * DRAW_CELL_SIZE + 4;
    int h = DRAW_GRID_H * DRAW_CELL_SIZE + 4;

    pt_draw_canvas_frame(x, y, w, h);
}

static void time_draw_full_grid(void)
{
    int x;
    int y;

    for (y = 0; y < DRAW_GRID_H; y++)
    {
        for (x = 0; x < DRAW_GRID_W; x++)
            time_draw_cell(x, y, time_draw_grid[time_draw_grid_index(x, y)]);
    }
}

static void time_apply_draw_background_if_ready(void)
{
    uint32_t ready;
    uint32_t seq;
    int x;
    int y;
    int changed = 0;

    ready = time_shared_read32(DRAW_BG_READY);
    if (ready == 0)
        return;

    seq = time_shared_read32(DRAW_BG_SEQ);
    if (seq == time_draw_last_bg_seq)
    {
        time_shared_write32(DRAW_BG_READY, 0);
        return;
    }

    /*
       Direct grid mirror sync.
       Debug KEY1 background and IMG draw micro-batches both write the same
       96x96 RGB332 grid. Redraw only changed cells.
    */
    for (y = 0; y < DRAW_GRID_H; y++)
    {
        for (x = 0; x < DRAW_GRID_W; x++)
        {
            int index = time_draw_grid_index(x, y);
            uint8_t color = time_shared_read8(DRAW_BG_GRID + y * DRAW_GRID_W + x);

            if (time_draw_grid[index] != color)
            {
                time_draw_grid[index] = color;
                time_draw_cell(x, y, color);
                changed++;
            }
        }
    }

    time_draw_last_bg_seq = seq;
    time_shared_write32(DRAW_BG_READY, 0);

    if (changed > 0)
    {
        recount_draw_colors();
        if (draw_count_black == (uint32_t)(DRAW_GRID_W * DRAW_GRID_H))
            trigger_sfx_flag(FLAG_SFX_CLEAR);
        printf("Draw: synced 96x96 RGB332 grid seq=%lu changed=%d\n",
               (unsigned long)seq,
               changed);
        fflush(stdout);
    }
}

static void time_apply_draw_mailbox(void)
{
    uint32_t ready;
    uint32_t seq;
    int x;
    int y;
    uint8_t color;

    ready = time_shared_read32(DRAW_MB_READY);
    if (ready == 0)
        return;

    seq = time_shared_read32(DRAW_MB_SEQ);
    if (seq == time_draw_last_seq)
    {
        time_shared_write32(DRAW_MB_ACK, seq);
        time_shared_write32(DRAW_MB_READY, 0);
        return;
    }

    x = (int)time_shared_read32(DRAW_MB_X);
    y = (int)time_shared_read32(DRAW_MB_Y);
    color = (uint8_t)(time_shared_read32(DRAW_MB_COLOR) & 0xFF);
    time_draw_last_seq = seq;

    if (!time_draw_valid_cell(x, y))
    {
        time_shared_write32(DRAW_MB_STATUS, DRAW_STATUS_BAD_COORD);
    }
    else
    {
        int idx = time_draw_grid_index(x, y);
        uint8_t old_color = time_draw_grid[idx];
        if (old_color != color)
        {
            count_color_add(old_color, -1);
            time_draw_grid[idx] = color;
            count_color_add(color, 1);
            publish_draw_counts();
        }
        time_draw_cell(x, y, color);
        time_shared_write32(DRAW_MB_STATUS, DRAW_STATUS_OK);
    }

    time_shared_write32(DRAW_MB_ACK, seq);
    time_shared_write32(DRAW_MB_READY, 0);
}

void __attribute__((weak)) draw_game_init(void)
{
    int i;

    for (i = 0; i < DRAW_GRID_W * DRAW_GRID_H; i++)
        time_draw_grid[i] = COL_BLACK;

    draw_count_red = draw_count_green = draw_count_blue = draw_count_yellow = 0;
    draw_count_black = DRAW_GRID_W * DRAW_GRID_H;
    draw_count_white = draw_count_other = 0;
    publish_draw_counts();

    time_draw_last_seq = 0;
    time_draw_last_bg_seq = 0;
    time_draw_rx_last_seq = time_shared_read32(FLAG_RT_ACTIVITY_SEQ);
    time_draw_rx_ticks = 0;
    time_draw_rx_indicator_state = -1;

    pt_draw_canvas_background();
    pt_print_shadow(92, 8, "PIXEL STUDIO", PT_GRASS_LIGHT);

    time_draw_rx_indicator(0);
    time_draw_border();
    time_draw_full_grid();

    /* Draw this after the frame/grid so it is not hidden by the canvas shadow. */
    pt_print_shadow(38, 222, "96X96 RGB332 LIVE CANVAS", PT_GOLD);

    time_shared_write32(DRAW_MB_READY, 0);
    time_shared_write32(DRAW_MB_ACK, 0);
    time_shared_write32(DRAW_MB_STATUS, DRAW_STATUS_OK);

    time_apply_draw_background_if_ready();

    printf("Draw pixel game start. 96x96 RGB332 mailbox at 0x%08X + 0x900\n", SHARED_FLAGS_BASE);
    fflush(stdout);
}

void __attribute__((weak)) draw_game_update(void)
{
    time_draw_update_rx_status();
    time_apply_draw_background_if_ready();
    time_apply_draw_mailbox();
}
