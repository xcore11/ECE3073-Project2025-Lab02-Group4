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
#include "io.h"

#include <stdint.h>
#include <stdio.h>

#define SHARED_FLAGS_BASE              0x05212000

#define FLAG_RT_ACTIVITY_SEQ           0x870
#define FLAG_RT_PANEL_MODE             0x874
#define GAME_MODE_DRAW                 2

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

#define DRAW_STATUS_OK                 0
#define DRAW_STATUS_BAD_COORD          1
#define DRAW_STATUS_BAD_COLOR          2

#define DRAW_GRID_X0                   64
#define DRAW_GRID_Y0                   24
#define DRAW_GRID_W                    96
#define DRAW_GRID_H                    96
#define DRAW_CELL_SIZE                 2

#define COL_BLACK                      0x00
#define COL_GREEN                      0x1C
#define COL_CYAN                       0x1F
#define COL_YELLOW                     0xFC

static uint32_t time_draw_last_seq = 0;
static uint32_t time_draw_last_bg_seq = 0;
static uint32_t time_draw_rx_last_seq = 0;
static int time_draw_rx_ticks = 0;
static int time_draw_rx_indicator_state = -1;
static uint8_t time_draw_grid[DRAW_GRID_W * DRAW_GRID_H];

static uint32_t time_shared_read32(uint32_t offset)
{
    return IORD_32DIRECT(SHARED_FLAGS_BASE, offset);
}

static void time_shared_write32(uint32_t offset, uint32_t value)
{
    IOWR_32DIRECT(SHARED_FLAGS_BASE, offset, value);
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

    vga_draw_rectangle(px, py, DRAW_CELL_SIZE, DRAW_CELL_SIZE, color);
}

static void time_draw_rx_indicator(int active)
{
    int color = active ? RX_ACTIVE_COLOR : RX_IDLE_COLOR;

    vga_draw_rectangle(10, 8, 44, 14, COL_BLACK);
    vga_draw_circle(16, 15, 4, color);
    vga_print_software_text(24, 10, active ? "RX" : "ID", color);
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

    vga_draw_rectangle(x, y, w, 2, COL_CYAN);
    vga_draw_rectangle(x, y + h - 2, w, 2, COL_CYAN);
    vga_draw_rectangle(x, y, 2, h, COL_CYAN);
    vga_draw_rectangle(x + w - 2, y, 2, h, COL_CYAN);
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
        time_draw_grid[time_draw_grid_index(x, y)] = color;
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

    time_draw_last_seq = 0;
    time_draw_last_bg_seq = 0;
    time_draw_rx_last_seq = time_shared_read32(FLAG_RT_ACTIVITY_SEQ);
    time_draw_rx_ticks = 0;
    time_draw_rx_indicator_state = -1;

    vga_fill_background(COL_BLACK);
    vga_print_software_text(90, 8, "DRAW PIXEL", COL_GREEN);
    vga_print_software_text(36, 220, "96X96 RGB332 REALTIME DRAW / DBG BG", COL_YELLOW);

    time_draw_rx_indicator(0);
    time_draw_border();
    time_draw_full_grid();

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
