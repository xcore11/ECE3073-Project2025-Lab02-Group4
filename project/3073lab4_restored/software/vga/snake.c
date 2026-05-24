#include "snake.h"
#include "vga.h"

#include "system.h"
#include "io.h"

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "alt_types.h"

/* accel.c provides these functions; no accel.h exists in the uploaded project. */
extern int accel_read_x(alt_32 *x);
extern int accel_read_y(alt_32 *y);
extern int accel_read_z(alt_32 *z);

/* =========================
   RGB332 VGA colors

   Use #ifndef so these do not redefine names if vga.h already provides them.
   Values are direct RGB332 bytes.
   ========================= */

#ifndef COL_BLACK
#define COL_BLACK   0x00
#endif
#ifndef COL_DARK
#define COL_DARK    0x49
#endif
#ifndef COL_BLUE
#define COL_BLUE    0x03
#endif
#ifndef COL_GREEN
#define COL_GREEN   0x1C
#endif
#ifndef COL_CYAN
#define COL_CYAN    0x1F
#endif
#ifndef COL_RED
#define COL_RED     0xE0
#endif
#ifndef COL_PURPLE
#define COL_PURPLE  0xE3
#endif
#ifndef COL_YELLOW
#define COL_YELLOW  0xFC
#endif
#ifndef COL_WHITE
#define COL_WHITE   0xFF
#endif

/* =========================
   Arena object colors
   ========================= */

#define SNAKE_COLOR_APPLE     COL_RED      /* expected red: 0x4 */
#define SNAKE_COLOR_WALL      COL_WHITE    /* expected white: 0xF */
#define SNAKE_COLOR_PORTAL_A  COL_BLUE     /* expected blue: 0x1 */
#define SNAKE_COLOR_PORTAL_B  COL_PURPLE   /* expected purple: 0x5 */
#define SNAKE_COLOR_HEAD      COL_YELLOW   /* known-good yellow */
#define SNAKE_COLOR_BODY      COL_GREEN    /* expected green: 0x2 */

/* =========================
   Snake settings
   ========================= */

#define CELL_SIZE 8

#define GRID_X0 32
#define GRID_Y0 40
#define GRID_W  32
#define GRID_H  22

#define MAX_SNAKE_LEN (GRID_W * GRID_H)
#define MAX_DECODER_APPLES 64

#define ACCEL_SNAKE_THRESHOLD 80
#define SNAKE_STEP_DELAY_US   120000
#define SNAKE_MAILBOX_SERVICE_SLICE_US 10000

/* =========================
   Shared SDRAM page

   IMPORTANT:
   These are OFFSETS from SHARED_FLAGS_BASE, not absolute addresses.
   Use IOWR_32DIRECT(SHARED_FLAGS_BASE, OFFSET, value).
   ========================= */

#define SHARED_FLAGS_BASE              0x05212000

#define FLAG_RT_ACTIVITY_SEQ           0x870
#define FLAG_RT_PANEL_MODE             0x874
#define GAME_MODE_SNAKE                1

/* Control peripheral/SFX mailbox. */
#define DEBUG_CONTROL_BASE             0x06000000
#define FLAG_CONTROL_EVENT_SEQ         0x800
#define FLAG_CONTROL_LAST_EVENT_TYPE   0x814
#define FLAG_CONTROL_LAST_EVENT_VALUE  0x818
#define FLAG_CONTROL_MESSAGE           0x828
#define DEBUG_CONTROL_MESSAGE_BYTES    68
#define DEBUG_CONTROL_CMD_BATCH        100
#define DEBUG_CONTROL_MASK_HEX_MESSAGE 0x00000001u

#define FLAG_SFX_EAT_APPLE             0xC40
#define FLAG_SFX_GAME_OVER             0xC44
#define FLAG_SFX_PORTAL                0xC48
#define FLAG_SFX_CLEAR                 0xC6C
#define FLAG_SFX_SNAKE_TURN            0xC70

#define RX_IDLE_COLOR                  0xE0
#define RX_ACTIVE_COLOR                0x1C
#define RX_PAUSE_TICKS                 30
#define SHARED_FLAGS_SIZE              0x1000

/* Snake status region: 0x040 - 0x0FF */
#define SNAKE_FLAG_MAGIC               0x040
#define SNAKE_FLAG_GAME_STATE          0x044
#define SNAKE_FLAG_SCORE               0x048
#define SNAKE_FLAG_HEAD_X              0x04C
#define SNAKE_FLAG_HEAD_Y              0x050
#define SNAKE_FLAG_EVENT_FLAGS         0x054
#define SNAKE_FLAG_ARENA_VERSION       0x058
#define SNAKE_FLAG_ARENA_DIRTY         0x05C
#define SNAKE_FLAG_APPLE_X             0x060
#define SNAKE_FLAG_APPLE_Y             0x064
#define SNAKE_FLAG_APPLE_COUNT         0x088  /* number of active apples mirrored in SNAKE_SHARED_APPLE_LIST */
#define SNAKE_FLAG_PORTAL_ENABLED      0x068
#define SNAKE_FLAG_PORTAL_A_X          0x06C
#define SNAKE_FLAG_PORTAL_A_Y          0x070
#define SNAKE_FLAG_PORTAL_B_X          0x074
#define SNAKE_FLAG_PORTAL_B_Y          0x078
#define SNAKE_FLAG_WALL_COUNT          0x07C
#define SNAKE_FLAG_LAST_APPLIED_SEQ    0x080
#define SNAKE_FLAG_LAST_CMD_STATUS     0x084

/* Snake binary mailbox: 0x100 - 0x2FF */
#define SNAKE_MB_READY                 0x100
#define SNAKE_MB_ACK                   0x104
#define SNAKE_MB_SEQ                   0x108
#define SNAKE_MB_TYPE                  0x10C
#define SNAKE_MB_COUNT                 0x110
#define SNAKE_MB_FLAGS                 0x114
#define SNAKE_MB_PAYLOAD_LEN           0x118
#define SNAKE_MB_PAYLOAD               0x120
#define SNAKE_MB_PAYLOAD_CAP           (0x300 - SNAKE_MB_PAYLOAD)

/* Snake arena mirror: 0x300 - 0x5BF, 704 bytes for 32x22 grid */
#define SNAKE_SHARED_ARENA_GRID        0x300
#define SNAKE_SHARED_ARENA_GRID_SIZE   (GRID_W * GRID_H)

/* Optional wall coordinate mirror: 0x600 onward, x,y byte pairs */
#define SNAKE_SHARED_WALL_LIST         0x600
#define SNAKE_SHARED_WALL_LIST_CAP     256

/* Optional apple coordinate mirror: 0x700 onward, x,y byte pairs */
#define SNAKE_SHARED_APPLE_LIST        0x700
#define SNAKE_SHARED_APPLE_LIST_CAP    (MAX_DECODER_APPLES * 2)

#define SNAKE_MAGIC_VALUE              0x534E414B  /* 'SNAK' */

/* Game states written to SNAKE_FLAG_GAME_STATE */
#define SNAKE_STATE_INACTIVE           0
#define SNAKE_STATE_RUNNING            1
#define SNAKE_STATE_LOST               2

/* Event flags OR-ed into SNAKE_FLAG_EVENT_FLAGS */
#define SNAKE_EVENT_ATE_APPLE          0x00000001
#define SNAKE_EVENT_HIT_WALL           0x00000002
#define SNAKE_EVENT_USED_PORTAL        0x00000004
#define SNAKE_EVENT_LOST               0x00000008
#define SNAKE_EVENT_ARENA_UPDATE       0x00000010
#define SNAKE_EVENT_BAD_COMMAND        0x00000020

/* Mailbox command types written by image/control processor */
#define SNAKE_CMD_NONE                 0
#define SNAKE_CMD_SET_APPLE            1  /* payload: x1,y1,x2,y2... supports multiple apples */
#define SNAKE_CMD_SET_WALLS            2  /* payload: x1,y1,x2,y2... replaces old walls */
#define SNAKE_CMD_ADD_WALLS            3  /* payload: x1,y1,x2,y2... keeps old walls */
#define SNAKE_CMD_SET_PORTALS          4  /* payload: ax,ay,bx,by */
#define SNAKE_CMD_CLEAR_ARENA          5  /* clears apple/walls/portals */
#define SNAKE_CMD_CLEAR_WALLS          6  /* clears only walls */
#define SNAKE_CMD_ADD_APPLES           7  /* payload: x1,y1,x2,y2... keeps old apples */
#define SNAKE_CMD_CLEAR_CELLS           8  /* payload: x1,y1,x2,y2... clears selected static cells */

#define SNAKE_CMD_STATUS_OK            0
#define SNAKE_CMD_STATUS_BAD_TYPE      1
#define SNAKE_CMD_STATUS_BAD_LENGTH    2
#define SNAKE_CMD_STATUS_BAD_COORD     3

/* Static arena cell types stored locally and mirrored to SDRAM */
#define SNAKE_CELL_EMPTY               0
#define SNAKE_CELL_WALL                1
#define SNAKE_CELL_APPLE               2
#define SNAKE_CELL_PORTAL_A            3
#define SNAKE_CELL_PORTAL_B            4

/* =========================
   Types
   ========================= */

typedef struct
{
    int x;
    int y;
} SnakeCell;

typedef enum
{
    DIR_UP = 0,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} SnakeDirection;

/* =========================
   Local fast state
   ========================= */

static SnakeCell snake[MAX_SNAKE_LEN];
static int snake_len = 0;

static SnakeCell food;
static int food_valid = 0;
static int active_apple_count = 0;

static SnakeCell portal_a;
static SnakeCell portal_b;
static int portal_enabled = 0;

static SnakeDirection current_dir = DIR_RIGHT;
static SnakeDirection next_dir = DIR_RIGHT;

static int score = 0;
static int snake_lost = 0;
static int first_full_draw_needed = 1;

static unsigned int snake_rand_seed = 12345u;

/* The active arena is checked from local RAM for speed. */
static uint8_t arena_grid[GRID_W * GRID_H];
static int wall_count = 0;

/*
   Decoder arena config is the persistent arena sent by the image/decoder
   processor through SDRAM mailbox commands.

   Important behavior:
       - snake retry/reset restores this config
       - walls/portals/decoder apple stay after retry
       - only explicit decoder clear commands remove them
*/
static uint8_t decoder_arena_grid[GRID_W * GRID_H];
static int decoder_config_valid = 0;
static SnakeCell decoder_apples[MAX_DECODER_APPLES];
static int decoder_apple_count = 0;
static SnakeCell decoder_portal_a;
static SnakeCell decoder_portal_b;
static int decoder_portal_enabled = 0;

/* Shared metadata state. */
static uint32_t arena_version = 0;
static uint32_t snake_event_flags = 0;
static uint32_t last_seen_mailbox_seq = 0;

/* For incremental redraw, so we do not clear the full screen every frame. */
static SnakeCell old_tail;
static SnakeCell old_head;
static int erase_tail_needed = 0;
static int ate_food_last_step = 0;
static int score_draw_needed = 1;
static int lose_screen_drawn = 0;

static uint32_t snake_rx_last_seq = 0;
static int snake_rx_pause_ticks = 0;
static int snake_rx_indicator_state = -1;
static int last_published_dir = -1;

/* =========================
   SDRAM helpers
   ========================= */

static void shared_write32(uint32_t offset, uint32_t value)
{
    IOWR_32DIRECT(SHARED_FLAGS_BASE, offset, value);
}

static uint32_t shared_read32(uint32_t offset)
{
    return IORD_32DIRECT(SHARED_FLAGS_BASE, offset);
}

static void shared_write8(uint32_t offset, uint8_t value)
{
    volatile uint8_t *p = (volatile uint8_t *)(SHARED_FLAGS_BASE + offset);
    *p = value;
}

static uint8_t shared_read8(uint32_t offset)
{
    volatile uint8_t *p = (volatile uint8_t *)(SHARED_FLAGS_BASE + offset);
    return *p;
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

static void trigger_sfx_flag(uint32_t flag_offset)
{
    shared_write32(flag_offset, shared_read32(flag_offset) + 1);
}

static const char *dir_name(SnakeDirection dir)
{
    if (dir == DIR_UP) return "UP";
    if (dir == DIR_DOWN) return "DOWN";
    if (dir == DIR_LEFT) return "LEFT";
    return "RIGHT";
}

static void publish_direction_if_changed(void)
{
    if ((int)current_dir != last_published_dir)
    {
        last_published_dir = (int)current_dir;
        trigger_sfx_flag(FLAG_SFX_SNAKE_TURN);
    }
}

static int valid_cell(int x, int y)
{
    return (x >= 0 && x < GRID_W && y >= 0 && y < GRID_H);
}

static void wrap_cell_to_arena(SnakeCell *cell)
{
    if (cell->x < 0)
        cell->x = GRID_W - 1;
    else if (cell->x >= GRID_W)
        cell->x = 0;

    if (cell->y < 0)
        cell->y = GRID_H - 1;
    else if (cell->y >= GRID_H)
        cell->y = 0;
}

static int grid_index(int x, int y)
{
    return y * GRID_W + x;
}

static void shared_mirror_cell(int x, int y)
{
    if (!valid_cell(x, y))
        return;

    shared_write8(SNAKE_SHARED_ARENA_GRID + grid_index(x, y),
                  arena_grid[grid_index(x, y)]);
}

static void shared_mirror_full_arena(void)
{
    int i;

    for (i = 0; i < SNAKE_SHARED_ARENA_GRID_SIZE; i++)
        shared_write8(SNAKE_SHARED_ARENA_GRID + i, arena_grid[i]);
}

static void shared_mirror_apple_list(void)
{
    int i;
    int written = 0;

    for (i = 0; i < GRID_W * GRID_H && written < MAX_DECODER_APPLES; i++)
    {
        if (arena_grid[i] == SNAKE_CELL_APPLE)
        {
            int x = i % GRID_W;
            int y = i / GRID_W;

            shared_write8(SNAKE_SHARED_APPLE_LIST + written * 2, (uint8_t)x);
            shared_write8(SNAKE_SHARED_APPLE_LIST + written * 2 + 1, (uint8_t)y);
            written++;
        }
    }

    shared_write32(SNAKE_FLAG_APPLE_COUNT, (uint32_t)written);

    for (; written < MAX_DECODER_APPLES; written++)
    {
        shared_write8(SNAKE_SHARED_APPLE_LIST + written * 2, 0xFFu);
        shared_write8(SNAKE_SHARED_APPLE_LIST + written * 2 + 1, 0xFFu);
    }
}

static void shared_set_event(uint32_t event_mask)
{
    snake_event_flags |= event_mask;
    shared_write32(SNAKE_FLAG_EVENT_FLAGS, snake_event_flags);
}

static void shared_publish_status(void)
{
    shared_write32(SNAKE_FLAG_MAGIC, SNAKE_MAGIC_VALUE);
    shared_write32(SNAKE_FLAG_GAME_STATE, snake_lost ? SNAKE_STATE_LOST : SNAKE_STATE_RUNNING);
    shared_write32(SNAKE_FLAG_SCORE, (uint32_t)score);

    if (snake_len > 0)
    {
        shared_write32(SNAKE_FLAG_HEAD_X, (uint32_t)snake[0].x);
        shared_write32(SNAKE_FLAG_HEAD_Y, (uint32_t)snake[0].y);
    }

    shared_write32(SNAKE_FLAG_EVENT_FLAGS, snake_event_flags);
    shared_write32(SNAKE_FLAG_ARENA_VERSION, arena_version);
    shared_write32(SNAKE_FLAG_ARENA_DIRTY, 0);

    if (food_valid)
    {
        shared_write32(SNAKE_FLAG_APPLE_X, (uint32_t)food.x);
        shared_write32(SNAKE_FLAG_APPLE_Y, (uint32_t)food.y);
    }
    else
    {
        shared_write32(SNAKE_FLAG_APPLE_X, 0xFFFFFFFFu);
        shared_write32(SNAKE_FLAG_APPLE_Y, 0xFFFFFFFFu);
    }

    shared_mirror_apple_list();

    shared_write32(SNAKE_FLAG_PORTAL_ENABLED, (uint32_t)portal_enabled);
    shared_write32(SNAKE_FLAG_PORTAL_A_X, portal_enabled ? (uint32_t)portal_a.x : 0xFFFFFFFFu);
    shared_write32(SNAKE_FLAG_PORTAL_A_Y, portal_enabled ? (uint32_t)portal_a.y : 0xFFFFFFFFu);
    shared_write32(SNAKE_FLAG_PORTAL_B_X, portal_enabled ? (uint32_t)portal_b.x : 0xFFFFFFFFu);
    shared_write32(SNAKE_FLAG_PORTAL_B_Y, portal_enabled ? (uint32_t)portal_b.y : 0xFFFFFFFFu);
    shared_write32(SNAKE_FLAG_WALL_COUNT, (uint32_t)wall_count);
    shared_write32(SNAKE_FLAG_LAST_APPLIED_SEQ, last_seen_mailbox_seq);
}

static void shared_init_snake_page(void)
{
    shared_write32(SNAKE_FLAG_MAGIC, SNAKE_MAGIC_VALUE);
    shared_write32(SNAKE_FLAG_EVENT_FLAGS, 0);
    shared_write32(SNAKE_FLAG_ARENA_DIRTY, 0);
    shared_write32(SNAKE_FLAG_LAST_CMD_STATUS, SNAKE_CMD_STATUS_OK);

    /* Do not clear READY here. Another processor may already be preparing a command. */
    shared_write32(SNAKE_MB_ACK, 0);

    shared_mirror_full_arena();
    shared_publish_status();
}

/* =========================
   Game helpers
   ========================= */

static unsigned int snake_rand(void)
{
    snake_rand_seed = (snake_rand_seed * 1103515245u) + 12345u;
    return (snake_rand_seed >> 16) & 0x7FFFu;
}

static int is_reverse_direction(SnakeDirection old_dir, SnakeDirection new_dir)
{
    if (old_dir == DIR_UP && new_dir == DIR_DOWN)
        return 1;

    if (old_dir == DIR_DOWN && new_dir == DIR_UP)
        return 1;

    if (old_dir == DIR_LEFT && new_dir == DIR_RIGHT)
        return 1;

    if (old_dir == DIR_RIGHT && new_dir == DIR_LEFT)
        return 1;

    return 0;
}

static int snake_occupies_cell(int x, int y)
{
    int i;

    for (i = 0; i < snake_len; i++)
    {
        if (snake[i].x == x && snake[i].y == y)
            return 1;
    }

    return 0;
}

static void arena_set_cell(int x, int y, uint8_t cell_value)
{
    if (!valid_cell(x, y))
        return;

    arena_grid[grid_index(x, y)] = cell_value;
    shared_mirror_cell(x, y);
}

static void draw_cell(int cell_x, int cell_y, unsigned char color)
{
    int px = GRID_X0 + (cell_x * CELL_SIZE);
    int py = GRID_Y0 + (cell_y * CELL_SIZE);

    vga_draw_rectangle(px, py, CELL_SIZE - 1, CELL_SIZE - 1, color);
}

static void draw_static_cell(int cell_x, int cell_y)
{
    uint8_t value;

    if (!valid_cell(cell_x, cell_y))
        return;

    value = arena_grid[grid_index(cell_x, cell_y)];

    if (value == SNAKE_CELL_WALL)
        draw_cell(cell_x, cell_y, SNAKE_COLOR_WALL);
    else if (value == SNAKE_CELL_APPLE)
        draw_cell(cell_x, cell_y, SNAKE_COLOR_APPLE);
    else if (value == SNAKE_CELL_PORTAL_A)
        draw_cell(cell_x, cell_y, SNAKE_COLOR_PORTAL_A);
    else if (value == SNAKE_CELL_PORTAL_B)
        draw_cell(cell_x, cell_y, SNAKE_COLOR_PORTAL_B);
    else
        draw_cell(cell_x, cell_y, COL_BLACK);
}

static void draw_static_cell_if_visible(int cell_x, int cell_y)
{
    if (!snake_occupies_cell(cell_x, cell_y))
        draw_static_cell(cell_x, cell_y);
}

static void draw_static_arena_full(void)
{
    int x;
    int y;

    for (y = 0; y < GRID_H; y++)
    {
        for (x = 0; x < GRID_W; x++)
        {
            if (arena_grid[grid_index(x, y)] != SNAKE_CELL_EMPTY)
                draw_static_cell(x, y);
        }
    }
}

static void draw_border(void)
{
    int x = GRID_X0 - 2;
    int y = GRID_Y0 - 2;
    int w = (GRID_W * CELL_SIZE) + 4;
    int h = (GRID_H * CELL_SIZE) + 4;

    vga_draw_rectangle(x, y, w, 2, COL_CYAN);
    vga_draw_rectangle(x, y + h - 2, w, 2, COL_CYAN);
    vga_draw_rectangle(x, y, 2, h, COL_CYAN);
    vga_draw_rectangle(x + w - 2, y, 2, h, COL_CYAN);
}

static void snake_draw_rx_indicator(int active)
{
    int color = active ? RX_ACTIVE_COLOR : RX_IDLE_COLOR;

    vga_draw_rectangle(12, 10, 42, 14, COL_BLACK);
    vga_draw_circle(18, 17, 4, color);
    vga_print_software_text(26, 12, active ? "RX" : "ID", active ? RX_ACTIVE_COLOR : RX_IDLE_COLOR);
}

static int snake_update_rx_status(void)
{
    uint32_t seq = shared_read32(FLAG_RT_ACTIVITY_SEQ);
    uint32_t panel = shared_read32(FLAG_RT_PANEL_MODE);
    int active;

    if (panel == GAME_MODE_SNAKE && seq != snake_rx_last_seq)
    {
        snake_rx_last_seq = seq;
        snake_rx_pause_ticks = RX_PAUSE_TICKS;
    }
    else if (snake_rx_pause_ticks > 0)
    {
        snake_rx_pause_ticks--;
    }

    active = (snake_rx_pause_ticks > 0);

    if (active != snake_rx_indicator_state)
    {
        snake_rx_indicator_state = active;
        snake_draw_rx_indicator(active);
    }

    return active;
}

static void draw_score(void)
{
    char score_text[32];

    /* Clear only score area, not the whole screen. */
    vga_draw_rectangle(200, 10, 112, 12, COL_BLACK);

    sprintf(score_text, "SCORE %d", score);
    vga_print_software_text(204, 12, score_text, COL_YELLOW);
}

static void draw_hud(void)
{
    vga_print_software_text(68, 12, "RETRO SNAKE", COL_GREEN);
    snake_rx_indicator_state = -1;
    snake_draw_rx_indicator(0);
    draw_score();

    vga_print_software_text(32, 224, "TILT TO MOVE", COL_CYAN);
    vga_print_software_text(184, 224, "SW9 MENU", COL_RED);
}

static void draw_snake_full(void)
{
    int i;

    for (i = 0; i < snake_len; i++)
    {
        if (i == 0)
            draw_cell(snake[i].x, snake[i].y, SNAKE_COLOR_HEAD);
        else
            draw_cell(snake[i].x, snake[i].y, SNAKE_COLOR_BODY);
    }
}

static void draw_game_screen_full_once(void)
{
    vga_fill_background(COL_BLACK);
    draw_hud();
    draw_border();
    draw_static_arena_full();
    draw_snake_full();
}

static void draw_game_screen_incremental(void)
{
    if (first_full_draw_needed)
    {
        draw_game_screen_full_once();
        first_full_draw_needed = 0;
        score_draw_needed = 0;
        return;
    }

    if (erase_tail_needed)
    {
        draw_static_cell(old_tail.x, old_tail.y);
        erase_tail_needed = 0;
    }

    /* Old head becomes body. */
    draw_cell(old_head.x, old_head.y, SNAKE_COLOR_BODY);

    /* New head. */
    draw_cell(snake[0].x, snake[0].y, SNAKE_COLOR_HEAD);

    if (ate_food_last_step)
    {
        score_draw_needed = 1;
        ate_food_last_step = 0;
    }

    if (score_draw_needed)
    {
        draw_score();
        score_draw_needed = 0;
    }
}

static void draw_lose_screen(void)
{
    char score_text[32];

    vga_fill_background(COL_BLACK);

    vga_draw_rectangle(0, 0, 320, 4, COL_RED);
    vga_draw_rectangle(0, 236, 320, 4, COL_RED);
    vga_draw_rectangle(0, 0, 4, 240, COL_RED);
    vga_draw_rectangle(316, 0, 4, 240, COL_RED);

    vga_print_software_text(96, 58, "YOU LOSE", COL_RED);

    sprintf(score_text, "FINAL SCORE %d", score);
    vga_print_software_text(76, 92, score_text, COL_YELLOW);

    vga_print_software_text(52, 142, "KEY1 RETRY", COL_GREEN);
    vga_print_software_text(54, 166, "SW9 BACK TO MENU", COL_CYAN);
}

static void clear_all_static_arena(void)
{
    int i;

    for (i = 0; i < GRID_W * GRID_H; i++)
        arena_grid[i] = SNAKE_CELL_EMPTY;

    wall_count = 0;
    food_valid = 0;
    active_apple_count = 0;
    portal_enabled = 0;
    shared_mirror_full_arena();
}


static void decoder_config_clear_all(void)
{
    int i;

    for (i = 0; i < GRID_W * GRID_H; i++)
        decoder_arena_grid[i] = SNAKE_CELL_EMPTY;

    decoder_apple_count = 0;
    decoder_portal_enabled = 0;
    decoder_config_valid = 0;
}

static void decoder_config_remove_type(uint8_t cell_type)
{
    int i;

    for (i = 0; i < GRID_W * GRID_H; i++)
    {
        if (decoder_arena_grid[i] == cell_type)
            decoder_arena_grid[i] = SNAKE_CELL_EMPTY;
    }
}

static void decoder_config_set_cell(int x, int y, uint8_t cell_value);

static void decoder_apples_clear(void)
{
    decoder_apple_count = 0;
    decoder_config_remove_type(SNAKE_CELL_APPLE);
}

static void decoder_apples_add(int x, int y)
{
    if (!valid_cell(x, y))
        return;

    if (decoder_apple_count < MAX_DECODER_APPLES)
    {
        decoder_apples[decoder_apple_count].x = x;
        decoder_apples[decoder_apple_count].y = y;
        decoder_apple_count++;
    }

    decoder_config_set_cell(x, y, SNAKE_CELL_APPLE);
}

static void active_apples_resync_first(void)
{
    int i;

    active_apple_count = 0;
    food_valid = 0;

    for (i = 0; i < GRID_W * GRID_H; i++)
    {
        if (arena_grid[i] == SNAKE_CELL_APPLE)
        {
            int x = i % GRID_W;
            int y = i / GRID_W;

            active_apple_count++;

            if (!food_valid)
            {
                food.x = x;
                food.y = y;
                food_valid = 1;
            }
        }
    }
}

static void decoder_config_set_cell(int x, int y, uint8_t cell_value)
{
    if (!valid_cell(x, y))
        return;

    decoder_arena_grid[grid_index(x, y)] = cell_value;
    decoder_config_valid = 1;
}

static void restore_active_arena_from_decoder_config(void)
{
    int i;

    wall_count = 0;
    food_valid = 0;
    active_apple_count = 0;
    portal_enabled = 0;

    for (i = 0; i < GRID_W * GRID_H; i++)
    {
        arena_grid[i] = decoder_config_valid ? decoder_arena_grid[i] : SNAKE_CELL_EMPTY;

        if (arena_grid[i] == SNAKE_CELL_WALL)
            wall_count++;
    }

    active_apples_resync_first();

    if (decoder_config_valid && decoder_portal_enabled)
    {
        portal_a = decoder_portal_a;
        portal_b = decoder_portal_b;
        portal_enabled = 1;
    }

    shared_mirror_full_arena();
}

static void spawn_food(void)
{
    int tries = 0;
    int x;
    int y;

    if (food_valid && valid_cell(food.x, food.y))
    {
        if (arena_grid[grid_index(food.x, food.y)] == SNAKE_CELL_APPLE)
        {
            arena_set_cell(food.x, food.y, SNAKE_CELL_EMPTY);
            draw_static_cell_if_visible(food.x, food.y);
        }
    }

    do
    {
        x = (int)(snake_rand() % GRID_W);
        y = (int)(snake_rand() % GRID_H);
        tries++;

        if (tries > 1000)
        {
            int found = 0;
            int sx;
            int sy;

            for (sy = 0; sy < GRID_H && !found; sy++)
            {
                for (sx = 0; sx < GRID_W && !found; sx++)
                {
                    if (!snake_occupies_cell(sx, sy) && arena_grid[grid_index(sx, sy)] == SNAKE_CELL_EMPTY)
                    {
                        x = sx;
                        y = sy;
                        found = 1;
                    }
                }
            }

            if (!found)
            {
                food_valid = 0;
                shared_write32(SNAKE_FLAG_APPLE_X, 0xFFFFFFFFu);
                shared_write32(SNAKE_FLAG_APPLE_Y, 0xFFFFFFFFu);
                return;
            }

            break;
        }

    } while (snake_occupies_cell(x, y) || arena_grid[grid_index(x, y)] != SNAKE_CELL_EMPTY);

    food.x = x;
    food.y = y;
    food_valid = 1;
    active_apple_count = 1;
    arena_set_cell(food.x, food.y, SNAKE_CELL_APPLE);
    draw_static_cell_if_visible(food.x, food.y);

    shared_write32(SNAKE_FLAG_APPLE_X, (uint32_t)food.x);
    shared_write32(SNAKE_FLAG_APPLE_Y, (uint32_t)food.y);
    shared_mirror_apple_list();
}

/* =========================
   Mailbox command application
   ========================= */

static void set_command_status(uint32_t status)
{
    shared_write32(SNAKE_FLAG_LAST_CMD_STATUS, status);

    if (status != SNAKE_CMD_STATUS_OK)
        shared_set_event(SNAKE_EVENT_BAD_COMMAND);
}

static int read_payload_xy(uint32_t payload_offset, uint32_t payload_len,
                           uint32_t count, uint32_t index, int *x, int *y)
{
    /*
       New realtime IMG decoder writes mailbox payload through 32-bit Avalon
       direct registers so coordinates are coherent between IMG and VGA CPUs:
           coordinate i: x at payload + i*8, y at payload + i*8 + 4

       Keep the old byte payload fallback so older IMG code still works.
    */
    if (count > 0 && payload_len >= count * 8)
    {
        *x = (int)shared_read32(payload_offset + index * 8);
        *y = (int)shared_read32(payload_offset + index * 8 + 4);
    }
    else
    {
        *x = (int)shared_read8(payload_offset + index * 2);
        *y = (int)shared_read8(payload_offset + index * 2 + 1);
    }

    return valid_cell(*x, *y);
}

static int read_payload_portals(uint32_t payload_offset, uint32_t payload_len,
                                int *ax, int *ay, int *bx, int *by)
{
    if (payload_len >= 16)
    {
        *ax = (int)shared_read32(payload_offset + 0);
        *ay = (int)shared_read32(payload_offset + 4);
        *bx = (int)shared_read32(payload_offset + 8);
        *by = (int)shared_read32(payload_offset + 12);
    }
    else if (payload_len >= 4)
    {
        *ax = (int)shared_read8(payload_offset + 0);
        *ay = (int)shared_read8(payload_offset + 1);
        *bx = (int)shared_read8(payload_offset + 2);
        *by = (int)shared_read8(payload_offset + 3);
    }
    else
    {
        return 0;
    }

    return valid_cell(*ax, *ay) && valid_cell(*bx, *by);
}

static void remove_old_apple(void)
{
    int i;

    for (i = 0; i < GRID_W * GRID_H; i++)
    {
        if (arena_grid[i] == SNAKE_CELL_APPLE)
        {
            int x = i % GRID_W;
            int y = i / GRID_W;
            arena_grid[i] = SNAKE_CELL_EMPTY;
            shared_mirror_cell(x, y);
            draw_static_cell_if_visible(x, y);
        }
    }

    food_valid = 0;
    active_apple_count = 0;
}

static void remove_old_portals(void)
{
    if (portal_enabled)
    {
        if (valid_cell(portal_a.x, portal_a.y) &&
            arena_grid[grid_index(portal_a.x, portal_a.y)] == SNAKE_CELL_PORTAL_A)
        {
            arena_set_cell(portal_a.x, portal_a.y, SNAKE_CELL_EMPTY);
            draw_static_cell_if_visible(portal_a.x, portal_a.y);
        }

        if (valid_cell(portal_b.x, portal_b.y) &&
            arena_grid[grid_index(portal_b.x, portal_b.y)] == SNAKE_CELL_PORTAL_B)
        {
            arena_set_cell(portal_b.x, portal_b.y, SNAKE_CELL_EMPTY);
            draw_static_cell_if_visible(portal_b.x, portal_b.y);
        }
    }

    portal_enabled = 0;
}

static void clear_walls_only(void)
{
    int x;
    int y;

    for (y = 0; y < GRID_H; y++)
    {
        for (x = 0; x < GRID_W; x++)
        {
            if (arena_grid[grid_index(x, y)] == SNAKE_CELL_WALL)
            {
                arena_set_cell(x, y, SNAKE_CELL_EMPTY);
                draw_static_cell_if_visible(x, y);
            }
        }
    }

    wall_count = 0;
}

static void apply_apples(uint32_t count, uint32_t payload_len, int replace_old_apples)
{
    uint32_t i;
    int placed_any = 0;

    if (payload_len < 2)
    {
        set_command_status(SNAKE_CMD_STATUS_BAD_LENGTH);
        return;
    }

    if (count == 0)
    {
        if ((payload_len % 8) == 0 && payload_len >= 8)
            count = payload_len / 8;
        else
            count = payload_len / 2;
    }

    if (payload_len < count * 2)
    {
        set_command_status(SNAKE_CMD_STATUS_BAD_LENGTH);
        return;
    }

    if (count > MAX_DECODER_APPLES)
        count = MAX_DECODER_APPLES;

    if (replace_old_apples)
    {
        remove_old_apple();
        decoder_apples_clear();
    }

    for (i = 0; i < count; i++)
    {
        int x;
        int y;
        uint8_t old_value;

        if (!read_payload_xy(SNAKE_MB_PAYLOAD, payload_len, count, i, &x, &y))
        {
            set_command_status(SNAKE_CMD_STATUS_BAD_COORD);
            continue;
        }

        old_value = arena_grid[grid_index(x, y)];

        /* If this apple position is already an apple, keep it and avoid duplicate count/list entries. */
        if (old_value == SNAKE_CELL_APPLE)
        {
            placed_any = 1;
            continue;
        }

        /*
           Do not let decoder apples replace walls or portals.

           The image decoder can receive apple and wall streams close together.
           If a noisy/partial apple coordinate lands on an existing wall cell,
           the old behavior changed that wall into an apple. That made VGA look
           like walls and apples were randomly replacing each other.

           Arena static objects are one cell value only, so conflicts must be
           resolved here. Current rule: first static object wins; overlapping
           apple requests are ignored.
        */
        if (old_value == SNAKE_CELL_WALL ||
            old_value == SNAKE_CELL_PORTAL_A ||
            old_value == SNAKE_CELL_PORTAL_B)
        {
            placed_any = 1;
            continue;
        }

        arena_set_cell(x, y, SNAKE_CELL_APPLE);
        draw_static_cell_if_visible(x, y);

        decoder_apples_add(x, y);
        placed_any = 1;
    }

    active_apples_resync_first();
    shared_mirror_apple_list();

    if (placed_any)
        set_command_status(SNAKE_CMD_STATUS_OK);
    else
        set_command_status(SNAKE_CMD_STATUS_BAD_COORD);
}

static void apply_set_apple(uint32_t count, uint32_t payload_len)
{
    /* Replace all previous decoder apples with the mailbox apple list. */
    apply_apples(count, payload_len, 1);
}

static void apply_add_apples(uint32_t count, uint32_t payload_len)
{
    /* Add mailbox apple positions while keeping existing apples. */
    apply_apples(count, payload_len, 0);
}

static void apply_walls(uint32_t count, uint32_t payload_len, int replace_old_walls)
{
    uint32_t i;
    uint32_t max_count;

    if (payload_len < count * 2)
    {
        set_command_status(SNAKE_CMD_STATUS_BAD_LENGTH);
        return;
    }

    max_count = SNAKE_SHARED_WALL_LIST_CAP / 2;
    if (count > max_count)
        count = max_count;

    if (replace_old_walls)
    {
        clear_walls_only();
        decoder_config_remove_type(SNAKE_CELL_WALL);
    }

    for (i = 0; i < count; i++)
    {
        int x;
        int y;
        uint8_t old_value;

        if (!read_payload_xy(SNAKE_MB_PAYLOAD, payload_len, count, i, &x, &y))
        {
            set_command_status(SNAKE_CMD_STATUS_BAD_COORD);
            continue;
        }

        old_value = arena_grid[grid_index(x, y)];

        if (old_value == SNAKE_CELL_WALL)
            continue;

        /*
           Do not let decoder walls replace apples or portals.

           This is the matching protection for apply_apples(). If apple and wall
           commands overlap or arrive in a noisy order, the later command must
           not visually convert the previous object type. Current rule: first
           static object wins; overlapping wall requests are ignored.
        */
        if (old_value == SNAKE_CELL_APPLE ||
            old_value == SNAKE_CELL_PORTAL_A ||
            old_value == SNAKE_CELL_PORTAL_B)
        {
            continue;
        }

        arena_set_cell(x, y, SNAKE_CELL_WALL);
        wall_count++;

        /* Persist decoder walls across snake retry/reset.
           Since conflicts were skipped above, this will not erase apples or
           portals from decoder_arena_grid. */
        decoder_config_set_cell(x, y, SNAKE_CELL_WALL);

        shared_write8(SNAKE_SHARED_WALL_LIST + (i * 2), (uint8_t)x);
        shared_write8(SNAKE_SHARED_WALL_LIST + (i * 2 + 1), (uint8_t)y);

        draw_static_cell_if_visible(x, y);

        printf("[SNAKE MB] wall applied x=%d y=%d wall_count=%d\n", x, y, wall_count);
        fflush(stdout);
    }

    active_apples_resync_first();
    set_command_status(SNAKE_CMD_STATUS_OK);
}

static void apply_set_portals(uint32_t payload_len)
{
    int ax;
    int ay;
    int bx;
    int by;
    uint8_t old_a;
    uint8_t old_b;

    if (!read_payload_portals(SNAKE_MB_PAYLOAD, payload_len, &ax, &ay, &bx, &by))
    {
        if (payload_len < 4)
            set_command_status(SNAKE_CMD_STATUS_BAD_LENGTH);
        else
            set_command_status(SNAKE_CMD_STATUS_BAD_COORD);
        return;
    }

    remove_old_portals();

    old_a = arena_grid[grid_index(ax, ay)];
    old_b = arena_grid[grid_index(bx, by)];

    if (old_a == SNAKE_CELL_WALL && wall_count > 0)
        wall_count--;
    if (old_b == SNAKE_CELL_WALL && wall_count > 0)
        wall_count--;

    if ((old_a == SNAKE_CELL_APPLE) || (old_b == SNAKE_CELL_APPLE))
    {
        /* One or both portal cells replace active apples.
           Resync the active apple metadata after writing portals. */
    }

    portal_a.x = ax;
    portal_a.y = ay;
    portal_b.x = bx;
    portal_b.y = by;
    portal_enabled = 1;

    arena_set_cell(portal_a.x, portal_a.y, SNAKE_CELL_PORTAL_A);
    arena_set_cell(portal_b.x, portal_b.y, SNAKE_CELL_PORTAL_B);
    active_apples_resync_first();

    /* Persist decoder portals across snake retry/reset. */
    decoder_config_remove_type(SNAKE_CELL_PORTAL_A);
    decoder_config_remove_type(SNAKE_CELL_PORTAL_B);

    if (decoder_arena_grid[grid_index(ax, ay)] == SNAKE_CELL_APPLE ||
        decoder_arena_grid[grid_index(bx, by)] == SNAKE_CELL_APPLE)
        decoder_apples_clear();

    decoder_portal_a = portal_a;
    decoder_portal_b = portal_b;
    decoder_portal_enabled = 1;
    decoder_config_set_cell(portal_a.x, portal_a.y, SNAKE_CELL_PORTAL_A);
    decoder_config_set_cell(portal_b.x, portal_b.y, SNAKE_CELL_PORTAL_B);

    draw_static_cell_if_visible(portal_a.x, portal_a.y);
    draw_static_cell_if_visible(portal_b.x, portal_b.y);

    set_command_status(SNAKE_CMD_STATUS_OK);
}


static void apply_clear_cells(uint32_t count, uint32_t payload_len)
{
    uint32_t i;

    if (count == 0)
    {
        if ((payload_len % 8) == 0 && payload_len >= 8)
            count = payload_len / 8;
        else
            count = payload_len / 2;
    }

    if (payload_len < count * 2)
    {
        set_command_status(SNAKE_CMD_STATUS_BAD_LENGTH);
        return;
    }

    for (i = 0; i < count; i++)
    {
        int x;
        int y;
        uint8_t old_value;

        if (!read_payload_xy(SNAKE_MB_PAYLOAD, payload_len, count, i, &x, &y))
        {
            set_command_status(SNAKE_CMD_STATUS_BAD_COORD);
            continue;
        }

        old_value = arena_grid[grid_index(x, y)];

        if (old_value == SNAKE_CELL_WALL && wall_count > 0)
            wall_count--;

        if (old_value == SNAKE_CELL_PORTAL_A || old_value == SNAKE_CELL_PORTAL_B)
        {
            remove_old_portals();
            decoder_config_remove_type(SNAKE_CELL_PORTAL_A);
            decoder_config_remove_type(SNAKE_CELL_PORTAL_B);
            decoder_portal_enabled = 0;
        }
        else
        {
            arena_set_cell(x, y, SNAKE_CELL_EMPTY);
            decoder_config_set_cell(x, y, SNAKE_CELL_EMPTY);
            draw_static_cell_if_visible(x, y);
        }
    }

    active_apples_resync_first();
    set_command_status(SNAKE_CMD_STATUS_OK);
}

static void apply_mailbox_command(uint32_t cmd_type, uint32_t count, uint32_t payload_len)
{
    if (payload_len > SNAKE_MB_PAYLOAD_CAP)
    {
        set_command_status(SNAKE_CMD_STATUS_BAD_LENGTH);
        return;
    }

    if (cmd_type == SNAKE_CMD_SET_APPLE)
    {
        apply_set_apple(count, payload_len);
    }
    else if (cmd_type == SNAKE_CMD_ADD_APPLES)
    {
        apply_add_apples(count, payload_len);
    }
    else if (cmd_type == SNAKE_CMD_SET_WALLS)
    {
        apply_walls(count, payload_len, 1);
    }
    else if (cmd_type == SNAKE_CMD_ADD_WALLS)
    {
        apply_walls(count, payload_len, 0);
    }
    else if (cmd_type == SNAKE_CMD_SET_PORTALS)
    {
        apply_set_portals(payload_len);
    }
    else if (cmd_type == SNAKE_CMD_CLEAR_ARENA)
    {
        trigger_sfx_flag(FLAG_SFX_CLEAR);
        clear_all_static_arena();
        decoder_config_clear_all();
        set_command_status(SNAKE_CMD_STATUS_OK);
    }
    else if (cmd_type == SNAKE_CMD_CLEAR_WALLS)
    {
        trigger_sfx_flag(FLAG_SFX_CLEAR);
        clear_walls_only();
        decoder_config_remove_type(SNAKE_CELL_WALL);
        set_command_status(SNAKE_CMD_STATUS_OK);
    }
    else if (cmd_type == SNAKE_CMD_CLEAR_CELLS)
    {
        trigger_sfx_flag(FLAG_SFX_CLEAR);
        apply_clear_cells(count, payload_len);
    }
    else
    {
        set_command_status(SNAKE_CMD_STATUS_BAD_TYPE);
        return;
    }

    arena_version++;
    shared_write32(SNAKE_FLAG_ARENA_VERSION, arena_version);
    shared_write32(SNAKE_FLAG_ARENA_DIRTY, 0);
    shared_set_event(SNAKE_EVENT_ARENA_UPDATE);
    shared_publish_status();
}

static void snake_check_mailbox(void)
{
    uint32_t ready;
    uint32_t seq;
    uint32_t cmd_type;
    uint32_t count;
    uint32_t payload_len;

    ready = shared_read32(SNAKE_MB_READY);
    if (ready == 0)
        return;

    seq = shared_read32(SNAKE_MB_SEQ);
    if (seq == last_seen_mailbox_seq)
    {
        shared_write32(SNAKE_MB_ACK, seq);
        shared_write32(SNAKE_MB_READY, 0);
        return;
    }

    cmd_type = shared_read32(SNAKE_MB_TYPE);
    count = shared_read32(SNAKE_MB_COUNT);
    payload_len = shared_read32(SNAKE_MB_PAYLOAD_LEN);

    last_seen_mailbox_seq = seq;

    apply_mailbox_command(cmd_type, count, payload_len);

    shared_write32(SNAKE_MB_ACK, seq);
    shared_write32(SNAKE_MB_READY, 0);
    shared_write32(SNAKE_FLAG_LAST_APPLIED_SEQ, seq);
}

/* =========================
   Input and movement
   ========================= */

static void read_snake_direction(void)
{
    alt_32 x = 0;
    alt_32 y = 0;
    alt_32 z = 0;

    if (accel_read_x(&x) != 0 ||
        accel_read_y(&y) != 0 ||
        accel_read_z(&z) != 0)
    {
        return;
    }

    if (y > ACCEL_SNAKE_THRESHOLD)
    {
        if (!is_reverse_direction(current_dir, DIR_DOWN))
            next_dir = DIR_DOWN;
    }
    else if (y < -ACCEL_SNAKE_THRESHOLD)
    {
        if (!is_reverse_direction(current_dir, DIR_UP))
            next_dir = DIR_UP;
    }
    else if (x > ACCEL_SNAKE_THRESHOLD)
    {
        /* Keep your fixed left/right mapping. */
        if (!is_reverse_direction(current_dir, DIR_LEFT))
            next_dir = DIR_LEFT;
    }
    else if (x < -ACCEL_SNAKE_THRESHOLD)
    {
        /* Keep your fixed left/right mapping. */
        if (!is_reverse_direction(current_dir, DIR_RIGHT))
            next_dir = DIR_RIGHT;
    }
}

static void set_lost(uint32_t reason_event)
{
    snake_lost = 1;
    lose_screen_drawn = 0;
    shared_set_event(reason_event | SNAKE_EVENT_LOST);
    trigger_sfx_flag(FLAG_SFX_GAME_OVER);
    publish_control_message("DEAD");
    shared_publish_status();
}

static void apply_portal_if_needed(SnakeCell *cell)
{
    if (!portal_enabled)
        return;

    if (cell->x == portal_a.x && cell->y == portal_a.y)
    {
        cell->x = portal_b.x;
        cell->y = portal_b.y;
        shared_set_event(SNAKE_EVENT_USED_PORTAL);
        trigger_sfx_flag(FLAG_SFX_PORTAL);
        publish_control_message("PORTAL");
    }
    else if (cell->x == portal_b.x && cell->y == portal_b.y)
    {
        cell->x = portal_a.x;
        cell->y = portal_a.y;
        shared_set_event(SNAKE_EVENT_USED_PORTAL);
        trigger_sfx_flag(FLAG_SFX_PORTAL);
        publish_control_message("PORTAL");
    }
}

static void snake_step(void)
{
    int i;
    SnakeCell new_head;
    uint8_t new_cell_value;

    ate_food_last_step = 0;
    erase_tail_needed = 0;

    current_dir = next_dir;
    publish_direction_if_changed();

    old_head = snake[0];
    old_tail = snake[snake_len - 1];

    new_head = snake[0];

    if (current_dir == DIR_UP)
        new_head.y--;
    else if (current_dir == DIR_DOWN)
        new_head.y++;
    else if (current_dir == DIR_LEFT)
        new_head.x--;
    else if (current_dir == DIR_RIGHT)
        new_head.x++;

    /*
       Boundary behavior:
       Hitting the edge does NOT lose the game anymore.
       The snake wraps around to the opposite side of the arena.
       Lose conditions are now only:
           1. hitting a custom wall cell
           2. hitting the snake body
    */
    wrap_cell_to_arena(&new_head);

    new_cell_value = arena_grid[grid_index(new_head.x, new_head.y)];

    if (new_cell_value == SNAKE_CELL_WALL)
    {
        set_lost(SNAKE_EVENT_HIT_WALL);
        return;
    }

    if (new_cell_value == SNAKE_CELL_PORTAL_A || new_cell_value == SNAKE_CELL_PORTAL_B)
    {
        apply_portal_if_needed(&new_head);

        wrap_cell_to_arena(&new_head);

        new_cell_value = arena_grid[grid_index(new_head.x, new_head.y)];

        if (new_cell_value == SNAKE_CELL_WALL)
        {
            set_lost(SNAKE_EVENT_HIT_WALL);
            return;
        }
    }

    /*
       Self-collision check.
       Moving into the current tail cell is allowed only when the snake is
       not eating, because the tail moves away during this step.
    */
    for (i = 1; i < snake_len; i++)
    {
        if (snake[i].x == new_head.x && snake[i].y == new_head.y)
        {
            if (!(new_cell_value != SNAKE_CELL_APPLE &&
                  i == snake_len - 1 &&
                  snake[i].x == old_tail.x &&
                  snake[i].y == old_tail.y))
            {
                set_lost(SNAKE_EVENT_LOST);
                return;
            }
        }
    }

    if (new_cell_value == SNAKE_CELL_APPLE)
    {
        ate_food_last_step = 1;
        score++;
        shared_set_event(SNAKE_EVENT_ATE_APPLE);
        trigger_sfx_flag(FLAG_SFX_EAT_APPLE);
        publish_control_message("APPLE");

        /* Remove only the eaten active apple. Decoder apple config is kept,
           so all decoder apples return after retry/reset. */
        arena_set_cell(new_head.x, new_head.y, SNAKE_CELL_EMPTY);
        if (active_apple_count > 0)
            active_apple_count--;

        if (snake_len < MAX_SNAKE_LEN)
            snake_len++;
    }
    else
    {
        erase_tail_needed = 1;
    }

    for (i = snake_len - 1; i > 0; i--)
        snake[i] = snake[i - 1];

    snake[0] = new_head;

    if (ate_food_last_step)
    {
        active_apples_resync_first();
        shared_mirror_apple_list();

        /* If the decoder placed multiple apples, do not spawn a random
           replacement until all active apples have been eaten. */
        if (!food_valid)
            spawn_food();
    }

    shared_publish_status();
}

/* =========================
   Public API
   ========================= */

void snake_init(void)
{
    int start_x = GRID_W / 2;
    int start_y = GRID_H / 2;

    snake_len = 4;

    snake[0].x = start_x;
    snake[0].y = start_y;
    snake[1].x = start_x - 1;
    snake[1].y = start_y;
    snake[2].x = start_x - 2;
    snake[2].y = start_y;
    snake[3].x = start_x - 3;
    snake[3].y = start_y;

    current_dir = DIR_RIGHT;
    next_dir = DIR_RIGHT;
    last_published_dir = (int)DIR_RIGHT;
    publish_control_message("SNAKE");

    score = 0;
    snake_lost = 0;
    first_full_draw_needed = 1;
    erase_tail_needed = 0;
    ate_food_last_step = 0;
    score_draw_needed = 1;
    lose_screen_drawn = 0;

    arena_version++;
    snake_event_flags = 0;

    /*
       Do not clear decoder-created arena objects on retry/reset.
       Restore the active arena from the persistent decoder config instead.
       If no decoder apple exists, spawn a normal random apple for gameplay.
    */
    restore_active_arena_from_decoder_config();

    snake_rand_seed = snake_rand_seed + 97u;
    if (!food_valid)
        spawn_food();

    last_seen_mailbox_seq = 0;
    snake_rx_last_seq = shared_read32(FLAG_RT_ACTIVITY_SEQ);
    snake_rx_pause_ticks = 0;
    snake_rx_indicator_state = -1;
    shared_init_snake_page();

    printf("Snake game start\n");
    printf("Snake SDRAM mailbox active at 0x%08X\n", SHARED_FLAGS_BASE);
    printf("Boundary mode = wrap around arena edges\n");
    printf("Lose condition = hit snake body or custom wall\n");
    printf("Mailbox apples: SET_APPLE replaces list, ADD_APPLES appends list\n");
    printf("Button during game = exit\n");
    printf("Button after lose = retry\n");
    fflush(stdout);

    draw_game_screen_incremental();
}

static void snake_service_mailbox_delay(unsigned int total_delay_us)
{
    unsigned int elapsed = 0;

    while (elapsed < total_delay_us)
    {
        unsigned int slice = SNAKE_MAILBOX_SERVICE_SLICE_US;

        if (elapsed + slice > total_delay_us)
            slice = total_delay_us - elapsed;

        snake_check_mailbox();
        usleep(slice);
        elapsed += slice;
    }

    snake_check_mailbox();
}

void snake_update(void)
{
    snake_check_mailbox();

    if (snake_lost)
    {
        if (!lose_screen_drawn)
        {
            printf("Snake lost. Final score = %d\n", score);
            printf("Press button to retry\n");
            fflush(stdout);

            draw_lose_screen();
            lose_screen_drawn = 1;
        }

        return;
    }

    if (snake_update_rx_status())
    {
        /*
           Keep servicing mailbox / drawing RX green, but pause snake movement
           until incoming realtime instructions settle. This prevents the snake
           from moving into a half-updated wall/apple/portal map.
        */
        snake_check_mailbox();
        usleep(20000);
        return;
    }

    read_snake_direction();
    snake_step();

    if (snake_lost)
    {
        draw_lose_screen();
        lose_screen_drawn = 1;
    }
    else
    {
        draw_game_screen_incremental();
    }

    snake_check_mailbox();
    snake_service_mailbox_delay(SNAKE_STEP_DELAY_US);
}

int snake_is_lost(void)
{
    return snake_lost != 0;
}

int snake_handle_button(void)
{
    if (snake_lost)
    {
        printf("Snake retry selected\n");
        fflush(stdout);

        snake_init();
        return 0;
    }

    printf("Snake exited by button\n");
    fflush(stdout);

    shared_publish_status();
    shared_write32(SNAKE_FLAG_GAME_STATE, SNAKE_STATE_INACTIVE);

    return 1;
}
