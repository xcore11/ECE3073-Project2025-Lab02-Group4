#include "ship.h"
#include "vga.h"
#include "pixel_theme.h"

#include "system.h"
#include "io.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "alt_types.h"

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
   Shared mailbox layout
   ========================= */
#define SHARED_FLAGS_BASE        0x05212000

#define FLAG_RT_ACTIVITY_SEQ     0x870
#define FLAG_RT_PANEL_MODE       0x874
#define GAME_MODE_BATTLE         4

#define DEBUG_CONTROL_BASE       0x06000000
#define FLAG_CONTROL_EVENT_SEQ   0x800
#define FLAG_CONTROL_LAST_EVENT_TYPE  0x814
#define FLAG_CONTROL_LAST_EVENT_VALUE 0x818
#define FLAG_CONTROL_MESSAGE     0x828
#define DEBUG_CONTROL_MESSAGE_BYTES 68
#define DEBUG_CONTROL_CMD_BATCH  100
#define DEBUG_CONTROL_MASK_HEX_MESSAGE 0x00000001u

#define FLAG_SFX_GAME_OVER       0xC44
#define FLAG_SFX_BATTLE_HIT      0xC4C
#define FLAG_SFX_BATTLE_MISS     0xC50
#define FLAG_SFX_CHANGE_ARSENAL  0xC54
#define FLAG_SFX_CLICK           0xC5C
#define FLAG_SFX_CLEAR           0xC6C

#define FLAG_BATTLE_LOADED_SHIP_CELLS 0xC80
#define FLAG_BATTLE_DESTROYED_CELLS   0xC84
#define FLAG_BATTLE_WIN               0xC88
#define FLAG_BATTLE_CROSS_LEFT        0xC8C
#define FLAG_BATTLE_SQUARE_LEFT       0xC90
#define FLAG_BATTLE_SELECTED_BOMB     0xC94

#define RX_IDLE_COLOR            0xE0
#define RX_ACTIVE_COLOR          0x1C
#define RX_ACTIVE_TICKS          30

#define BATTLE_MB_READY          0xA00
#define BATTLE_MB_ACK            0xA04
#define BATTLE_MB_SEQ            0xA08
#define BATTLE_MB_TYPE           0xA0C
#define BATTLE_MB_X              0xA10
#define BATTLE_MB_Y              0xA14
#define BATTLE_MB_FLAGS          0xA18
#define BATTLE_MB_STATUS         0xA1C

#define BATTLE_CMD_NONE          0
#define BATTLE_CMD_SET_SHIP      1
#define BATTLE_CMD_CLEAR_CELL    2
#define BATTLE_CMD_CLEAR_ALL     3

/* Direct hidden-map mirror written by IMG decoder. */
#define BATTLE_GRID_SEQ           0xA40
#define BATTLE_GRID_READY         0xA44
#define BATTLE_GRID_BASE          0xA80

/* =========================
   Control flags
   ========================= */
#define CONTROL_EVENT_NONE       0
#define CONTROL_EVENT_KEY        1
#define CONTROL_EVENT_SWITCH     2

#define CONTROL_KEY0_MASK        0x00000001u
#define CONTROL_KEY1_MASK        0x00000002u
#define CONTROL_SW5_MASK         0x00000020u
#define CONTROL_SW6_MASK         0x00000040u
#define CONTROL_SW7_MASK         0x00000080u
#define CONTROL_SW8_MASK         0x00000100u
#define CONTROL_SW9_MASK         0x00000200u

/* =========================
   Battle board settings
   ========================= */
#define BATTLE_W                 10
#define BATTLE_H                 10
#define CELL_SIZE                16
#define BOARD_X0                 24
#define BOARD_Y0                 36
#define BOARD_W_PX               (BATTLE_W * CELL_SIZE)
#define BOARD_H_PX               (BATTLE_H * CELL_SIZE)

#define PANEL_X0                 196
#define PANEL_Y0                 12
#define PANEL_W                  118
#define PANEL_H                  216

#define CURSOR_MOVE_THRESHOLD    80
#define CURSOR_REPEAT_US         120000
#define FRAME_DELAY_US           20000

#define CROSS_BOMB_MAX           3
#define SQUARE_BOMB_MAX          2
#define MAX_ANIMATIONS           24

#define BATTLE_CELL_EMPTY        0
#define BATTLE_CELL_SHIP         1

#define SHOT_NONE                0
#define SHOT_MISS                1
#define SHOT_HIT                 2

#define BOMB_STANDARD            0
#define BOMB_CROSS               1
#define BOMB_SQUARE              2

#define REVEAL_SWEEP_STEP_US     70000

typedef struct
{
    int active;
    int gx;
    int gy;
    int kind;
    int ttl;
} BlastAnim;

/* =========================
   Game state
   ========================= */
static uint8_t hidden_map[BATTLE_H][BATTLE_W];
static uint8_t shot_map[BATTLE_H][BATTLE_W];
static uint8_t dirty_cell[BATTLE_H][BATTLE_W];
static BlastAnim blasts[MAX_ANIMATIONS];

static int cursor_x = 0;
static int cursor_y = 0;
static int old_cursor_x = 0;
static int old_cursor_y = 0;

static int loaded_ship_cells = 0;
static int destroyed_ship_cells = 0;
static int layout_loaded = 0;
static int fleet_popup_visible = 0;
static int fleet_popup_drawn = 0;
static int fleet_popup_dismissed = 0;

static int reveal_enabled = 0;
static int reveal_anim_active = 0;
static int reveal_columns = 0;

static int selected_bomb = BOMB_STANDARD;
static int cross_bombs_left = CROSS_BOMB_MAX;
static int square_bombs_left = SQUARE_BOMB_MAX;

static uint32_t last_battle_seq = 0;
static uint32_t last_battle_grid_seq = 0;
static uint32_t last_switch_state = 0;
static uint32_t battle_rx_last_seq = 0;
static int battle_rx_ticks = 0;
static int battle_rx_indicator_active = 0;

static unsigned long fake_time_us = 0;
static unsigned long last_cursor_move_us = 0;
static unsigned long last_reveal_step_us = 0;

static int full_redraw_needed = 1;
static int hud_dirty = 1;
static int board_dirty = 1;
static int battle_win_sfx_sent = 0;

/* =========================
   SDRAM helpers
   ========================= */
static uint32_t shared_read32(uint32_t offset)
{
    return IORD_32DIRECT(SHARED_FLAGS_BASE, offset);
}

static void shared_write32(uint32_t offset, uint32_t value)
{
    IOWR_32DIRECT(SHARED_FLAGS_BASE, offset, value);
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

static void clear_battle_grid_mirror_memory(void)
{
    int i;

    shared_write32(BATTLE_GRID_READY, 0);
    shared_write32(BATTLE_GRID_SEQ, 0);

    for (i = 0; i < BATTLE_W * BATTLE_H; i++)
        shared_write32(BATTLE_GRID_BASE + (uint32_t)i * 4u, 0);
}

static const char *bomb_name_for_control(void)
{
    if (selected_bomb == BOMB_CROSS) return "CR";
    if (selected_bomb == BOMB_SQUARE) return "SQ";
    return "STD";
}

static void publish_battle_status_to_control(void)
{
    char msg[DEBUG_CONTROL_MESSAGE_BYTES];
    uint32_t win = (layout_loaded && loaded_ship_cells > 0 && destroyed_ship_cells >= loaded_ship_cells) ? 1u : 0u;

    if (cross_bombs_left < 0 || cross_bombs_left > CROSS_BOMB_MAX)
        cross_bombs_left = CROSS_BOMB_MAX;
    if (square_bombs_left < 0 || square_bombs_left > SQUARE_BOMB_MAX)
        square_bombs_left = SQUARE_BOMB_MAX;

    shared_write32(FLAG_BATTLE_LOADED_SHIP_CELLS, (uint32_t)loaded_ship_cells);
    shared_write32(FLAG_BATTLE_DESTROYED_CELLS, (uint32_t)destroyed_ship_cells);
    shared_write32(FLAG_BATTLE_WIN, win);
    shared_write32(FLAG_BATTLE_CROSS_LEFT, (uint32_t)cross_bombs_left);
    shared_write32(FLAG_BATTLE_SQUARE_LEFT, (uint32_t)square_bombs_left);
    shared_write32(FLAG_BATTLE_SELECTED_BOMB, (uint32_t)selected_bomb);

    if (win) {
        snprintf(msg, sizeof(msg), "WIN");
    } else {
        snprintf(msg, sizeof(msg), "ARS %s SQ%d CR%d",
                 bomb_name_for_control(), square_bombs_left, cross_bombs_left);
    }

    publish_control_message(msg);
}

static void show_fleet_down_popup_once(void)
{
    if (!fleet_popup_visible)
        fleet_popup_drawn = 0;

    fleet_popup_visible = 1;
    hud_dirty = 1;
}

static unsigned long tick_us(void)
{
    fake_time_us += FRAME_DELAY_US;
    return fake_time_us;
}

/* =========================
   Basic helpers
   ========================= */
static int in_bounds(int x, int y)
{
    return (x >= 0 && x < BATTLE_W && y >= 0 && y < BATTLE_H);
}

static int cell_left(int gx)
{
    return BOARD_X0 + gx * CELL_SIZE;
}

static int cell_top(int gy)
{
    return BOARD_Y0 + gy * CELL_SIZE;
}

static void mark_cell_dirty(int gx, int gy)
{
    if (!in_bounds(gx, gy))
        return;

    dirty_cell[gy][gx] = 1;
    board_dirty = 1;
}

static void mark_all_cells_dirty(void)
{
    int x;
    int y;

    for (y = 0; y < BATTLE_H; y++)
    {
        for (x = 0; x < BATTLE_W; x++)
            dirty_cell[y][x] = 1;
    }

    board_dirty = 1;
}

static void recalc_ship_counts(void)
{
    int x;
    int y;
    int total = 0;
    int destroyed = 0;

    for (y = 0; y < BATTLE_H; y++)
    {
        for (x = 0; x < BATTLE_W; x++)
        {
            if (hidden_map[y][x] == BATTLE_CELL_SHIP)
            {
                total++;
                if (shot_map[y][x] == SHOT_HIT)
                    destroyed++;
            }
        }
    }

    loaded_ship_cells = total;
    destroyed_ship_cells = destroyed;
    layout_loaded = (total > 0);

    if (layout_loaded && destroyed_ship_cells >= loaded_ship_cells && !fleet_popup_dismissed)
    {
        show_fleet_down_popup_once();
        if (!battle_win_sfx_sent)
        {
            battle_win_sfx_sent = 1;
            trigger_sfx_flag(FLAG_SFX_GAME_OVER);
        }
    }

    publish_battle_status_to_control();
    hud_dirty = 1;
}

static void reset_round_keep_layout(void)
{
    memset(shot_map, 0, sizeof(shot_map));
    memset(blasts, 0, sizeof(blasts));

    destroyed_ship_cells = 0;
    cross_bombs_left = CROSS_BOMB_MAX;
    square_bombs_left = SQUARE_BOMB_MAX;
    cursor_x = 0;
    cursor_y = 0;
    old_cursor_x = 0;
    old_cursor_y = 0;

    reveal_enabled = 0;
    reveal_anim_active = 0;
    reveal_columns = 0;
    selected_bomb = BOMB_STANDARD;
    fleet_popup_visible = 0;
    fleet_popup_drawn = 0;
    fleet_popup_dismissed = 0;
    battle_win_sfx_sent = 0;

    publish_battle_status_to_control();
    mark_all_cells_dirty();
    hud_dirty = 1;
}

static void clear_layout_and_round(void)
{
    memset(hidden_map, 0, sizeof(hidden_map));
    memset(shot_map, 0, sizeof(shot_map));
    memset(blasts, 0, sizeof(blasts));

    layout_loaded = 0;
    loaded_ship_cells = 0;
    destroyed_ship_cells = 0;
    fleet_popup_visible = 0;
    fleet_popup_drawn = 0;
    fleet_popup_dismissed = 0;
    battle_win_sfx_sent = 0;

    reset_round_keep_layout();

    full_redraw_needed = 1;
    hud_dirty = 1;
}

/* =========================
   Drawing primitives
   ========================= */
static void draw_bevel_box(int x, int y, int w, int h, int fill)
{
    pt_draw_shadow_box(x, y, w, h, (vga_color_t)fill, PT_GOLD, PT_SHADOW);
}

static void draw_neon_box(int x, int y, int w, int h, int fill, int edge)
{
    pt_draw_shadow_box(x, y, w, h, (vga_color_t)fill, (vga_color_t)edge, PT_SHADOW);
}

static void draw_sidebar_text_line(int y, const char *text, int color)
{
    /*
       VGA software text is not size-configurable in the current vga.c API.
       Keep every string short enough for the right 124px panel instead.
       Approx 8px/char means max safe length is about 13 chars.
    */
    vga_print_software_text(PANEL_X0 + 6, y, text, color);
}

static int reveal_visible_for_cell(int gx)
{
    return (reveal_enabled && gx < reveal_columns);
}

static void draw_water_tile(int px, int py, int miss)
{
    pt_draw_battle_ocean_tile(px, py, CELL_SIZE, miss);
}

static void draw_ship_tile(int px, int py, int revealed, int hit)
{
    pt_draw_battle_ship_tile(px, py, CELL_SIZE, revealed, hit);
}

static void draw_cell_base(int gx, int gy)
{
    int px = cell_left(gx);
    int py = cell_top(gy);
    int ship = (hidden_map[gy][gx] == BATTLE_CELL_SHIP);
    int shot = shot_map[gy][gx];
    int revealed = reveal_visible_for_cell(gx);

    /* Cell frame + water body. */
    draw_water_tile(px, py, shot == SHOT_MISS);

    if (ship && (revealed || shot == SHOT_HIT))
        draw_ship_tile(px, py, revealed, shot == SHOT_HIT);
    else if (shot == SHOT_HIT)
        draw_ship_tile(px, py, 0, 1);
}

static void draw_cursor(void)
{
    int px = cell_left(cursor_x);
    int py = cell_top(cursor_y);

    pt_draw_battle_cursor(px, py, CELL_SIZE);
}

static void draw_active_blasts(void)
{
    int i;

    for (i = 0; i < MAX_ANIMATIONS; i++)
    {
        int px;
        int py;
        int r;

        if (!blasts[i].active)
            continue;

        px = cell_left(blasts[i].gx) + CELL_SIZE / 2;
        py = cell_top(blasts[i].gy) + CELL_SIZE / 2;
        r = 1 + (8 - blasts[i].ttl);

        if (r < 1) r = 1;
        if (r > 7) r = 7;

        pt_draw_battle_blast(px, py, r, blasts[i].kind);
    }
}

static void draw_static_frame(void)
{
    int y;
    int x;

    pt_draw_battle_background();

    /* Left ocean frame. Draw once, not every update. */
    pt_draw_battle_board_backplate(2, 10, 184, 220);

    /* Push the logo upward so it no longer covers the X-axis labels. */
    pt_print_shadow(34, 2, "SEA RAIDERS", PT_GOLD);
    pt_print_shadow(66, 14, "COVE", PT_CREAM);

    /* Board backplate. */
    pt_draw_battle_board_backplate(BOARD_X0 - 12, BOARD_Y0 - 5, BOARD_W_PX + 17, BOARD_H_PX + 10);

    /* Grid labels. */
    for (x = 0; x < BATTLE_W; x++)
    {
        char s[2];
        s[0] = (char)('0' + x);
        s[1] = '\0';
        vga_print_software_text(cell_left(x) + 5, BOARD_Y0 - 11, s, PT_GOLD);
    }

    for (y = 0; y < BATTLE_H; y++)
    {
        char s[2];
        s[0] = (char)('A' + y);
        s[1] = '\0';
        vga_print_software_text(BOARD_X0 - 17, cell_top(y) + 4, s, PT_GOLD);
    }

    /* Decorative ship fills the lower empty area under the board. */
    pt_draw_battle_bottom_ship(66, 199);

    /* Right HUD frame. */
    pt_draw_battle_panel(PANEL_X0, PANEL_Y0, PANEL_W, PANEL_H);

    mark_all_cells_dirty();
    hud_dirty = 1;
}

static const char *bomb_name(void)
{
    if (selected_bomb == BOMB_CROSS) return "CROSS";
    if (selected_bomb == BOMB_SQUARE) return "SQUARE";
    return "STD";
}

static void draw_hud_bar(int y, int color, int value, int max_value)
{
    int w;

    if (max_value <= 0)
        max_value = 1;

    w = (value * 76) / max_value;
    if (w < 0) w = 0;
    if (w > 76) w = 76;

    vga_draw_rectangle(PANEL_X0 + 8, y, 78, 5, PT_SHADOW);
    vga_draw_rectangle(PANEL_X0 + 8, y, w, 5, color);
    vga_draw_rectangle(PANEL_X0 + 8, y, 78, 1, PT_CREAM);
}

static void battle_draw_rx_indicator(void)
{
    int color = battle_rx_indicator_active ? RX_ACTIVE_COLOR : RX_IDLE_COLOR;

    pt_draw_rx_badge(PANEL_X0 + 72, 18, battle_rx_indicator_active, "RX");
}

static void battle_update_rx_status(void)
{
    uint32_t seq = shared_read32(FLAG_RT_ACTIVITY_SEQ);
    uint32_t panel = shared_read32(FLAG_RT_PANEL_MODE);
    int active;

    if (panel == GAME_MODE_BATTLE && seq != battle_rx_last_seq)
    {
        battle_rx_last_seq = seq;
        battle_rx_ticks = RX_ACTIVE_TICKS;
    }
    else if (battle_rx_ticks > 0)
    {
        battle_rx_ticks--;
    }

    active = (battle_rx_ticks > 0);
    if (active != battle_rx_indicator_active)
    {
        battle_rx_indicator_active = active;
        hud_dirty = 1;
    }
}

static void draw_hud(void)
{
    char line[32];
    int total = loaded_ship_cells > 0 ? loaded_ship_cells : 1;

    /* Clear only HUD interior, not entire screen. */
    vga_draw_rectangle(PANEL_X0 + 4, PANEL_Y0 + 4, PANEL_W - 8, PANEL_H - 8, PT_INK);

    draw_sidebar_text_line(20, "SEA TOOLS", PT_GOLD);
    battle_draw_rx_indicator();

    snprintf(line, sizeof(line), "MODE:%s", bomb_name());
    draw_sidebar_text_line(38, line, PT_CREAM);

    pt_draw_battle_bomb_icon(PANEL_X0 + 8, 54, 0, selected_bomb == BOMB_STANDARD);
    pt_draw_battle_bomb_icon(PANEL_X0 + 45, 54, 2, selected_bomb == BOMB_SQUARE);
    pt_draw_battle_bomb_icon(PANEL_X0 + 82, 54, 1, selected_bomb == BOMB_CROSS);

    draw_sidebar_text_line(80, "S5   S6   S7", PT_WATER_LIGHT);

    snprintf(line, sizeof(line), "SQ:%d CR:%d", square_bombs_left, cross_bombs_left);
    draw_sidebar_text_line(92, line, PT_CREAM);

    draw_sidebar_text_line(108, "DMG", PT_RED);
    draw_hud_bar(119, PT_RED, destroyed_ship_cells, total);

    draw_sidebar_text_line(132, "FLEET", PT_GRASS_LIGHT);
    draw_hud_bar(143, PT_GRASS_LIGHT, loaded_ship_cells, 30);

    snprintf(line, sizeof(line), "CUR:%02d,%02d", cursor_x, cursor_y);
    draw_sidebar_text_line(156, line, PT_WHITE);

    snprintf(line, sizeof(line), "HIT:%02d/%02d", destroyed_ship_cells, loaded_ship_cells);
    draw_sidebar_text_line(170, line, PT_WHITE);

    if (!layout_loaded)
        draw_sidebar_text_line(184, "WAIT MAP", PT_RED);
    else if (destroyed_ship_cells >= loaded_ship_cells && loaded_ship_cells > 0)
        draw_sidebar_text_line(184, "FLEET DOWN", PT_GOLD);
    else if (reveal_enabled)
        draw_sidebar_text_line(184, "REVEAL ON", PT_GRASS_LIGHT);
    else
        draw_sidebar_text_line(184, "HIDDEN", PT_GRASS_LIGHT);

    draw_sidebar_text_line(196, "K0 USE", PT_WATER_LIGHT);
    draw_sidebar_text_line(208, "K1 REVEAL", PT_WATER_LIGHT);
    draw_sidebar_text_line(220, "SW9 MENU", PT_WATER_LIGHT);
}


static void draw_fleet_down_popup(void)
{
    pt_draw_battle_win_popup();
}

static void redraw_dirty_cells_and_overlays(void)
{
    int x;
    int y;

    if (full_redraw_needed)
    {
        draw_static_frame();
        full_redraw_needed = 0;
    }

    if (board_dirty)
    {
        for (y = 0; y < BATTLE_H; y++)
        {
            for (x = 0; x < BATTLE_W; x++)
            {
                if (!dirty_cell[y][x])
                    continue;

                draw_cell_base(x, y);
                dirty_cell[y][x] = 0;
            }
        }
        board_dirty = 0;
    }

    draw_active_blasts();
    draw_cursor();

    if (hud_dirty)
    {
        draw_hud();
        hud_dirty = 0;
    }

    if (fleet_popup_visible && !fleet_popup_drawn)
    {
        draw_fleet_down_popup();
        fleet_popup_drawn = 1;
    }
}

/* =========================
   Gameplay
   ========================= */
static void add_blast(int gx, int gy, int kind)
{
    int i;
    int index = -1;

    if (!in_bounds(gx, gy))
        return;

    for (i = 0; i < MAX_ANIMATIONS; i++)
    {
        if (!blasts[i].active)
        {
            index = i;
            break;
        }
    }

    if (index < 0)
        index = 0;

    blasts[index].active = 1;
    blasts[index].gx = gx;
    blasts[index].gy = gy;
    blasts[index].kind = kind;
    blasts[index].ttl = 8;

    mark_cell_dirty(gx, gy);
}

static void apply_hit_to_cell(int gx, int gy, int blast_kind)
{
    if (!in_bounds(gx, gy))
        return;

    if (hidden_map[gy][gx] == BATTLE_CELL_SHIP)
    {
        if (shot_map[gy][gx] != SHOT_HIT)
        {
            shot_map[gy][gx] = SHOT_HIT;
            destroyed_ship_cells++;
            trigger_sfx_flag(FLAG_SFX_BATTLE_HIT);

            if (layout_loaded && destroyed_ship_cells >= loaded_ship_cells && !fleet_popup_dismissed)
            {
                memset(blasts, 0, sizeof(blasts));
                show_fleet_down_popup_once();
                if (!battle_win_sfx_sent)
                {
                    battle_win_sfx_sent = 1;
                    trigger_sfx_flag(FLAG_SFX_GAME_OVER);
                }
            }

            publish_battle_status_to_control();
            hud_dirty = 1;
        }
    }
    else if (shot_map[gy][gx] == SHOT_NONE)
    {
        shot_map[gy][gx] = SHOT_MISS;
        trigger_sfx_flag(FLAG_SFX_BATTLE_MISS);
    }

    add_blast(gx, gy, blast_kind);
}

static void fire_standard(void)
{
    apply_hit_to_cell(cursor_x, cursor_y, 0);
}

static void fire_cross(void)
{
    apply_hit_to_cell(cursor_x, cursor_y, 1);
    apply_hit_to_cell(cursor_x - 1, cursor_y, 1);
    apply_hit_to_cell(cursor_x + 1, cursor_y, 1);
    apply_hit_to_cell(cursor_x, cursor_y - 1, 1);
    apply_hit_to_cell(cursor_x, cursor_y + 1, 1);
}

static void fire_square(void)
{
    int dx;
    int dy;

    for (dy = -1; dy <= 1; dy++)
    {
        for (dx = -1; dx <= 1; dx++)
            apply_hit_to_cell(cursor_x + dx, cursor_y + dy, 2);
    }
}

static void fire_selected_bomb(void)
{
    if (!layout_loaded)
    {
        printf("[BATTLE] KEY0 ignored, no ship layout loaded\n");
        fflush(stdout);
        return;
    }

    if (selected_bomb == BOMB_CROSS)
    {
        if (cross_bombs_left <= 0)
        {
            printf("[BATTLE] cross bomb unavailable\n");
            fflush(stdout);
            return;
        }
        cross_bombs_left--;
        hud_dirty = 1;
        fire_cross();
    }
    else if (selected_bomb == BOMB_SQUARE)
    {
        if (square_bombs_left <= 0)
        {
            printf("[BATTLE] square bomb unavailable\n");
            fflush(stdout);
            return;
        }
        square_bombs_left--;
        hud_dirty = 1;
        fire_square();
    }
    else
    {
        fire_standard();
    }

    publish_battle_status_to_control();

    printf("[BATTLE] fired mode=%d x=%d y=%d hit=%d/%d\n",
           selected_bomb, cursor_x, cursor_y,
           destroyed_ship_cells, loaded_ship_cells);
    fflush(stdout);
}

static void update_blasts(void)
{
    int i;

    for (i = 0; i < MAX_ANIMATIONS; i++)
    {
        if (!blasts[i].active)
            continue;

        mark_cell_dirty(blasts[i].gx, blasts[i].gy);

        blasts[i].ttl--;

        if (blasts[i].ttl <= 0)
        {
            blasts[i].active = 0;
        }
        else
        {
            mark_cell_dirty(blasts[i].gx, blasts[i].gy);
        }
    }
}


/* =========================
   Direct hidden-map mirror from IMG decoder
   ========================= */
static void sync_battle_grid_mirror(int force)
{
    uint32_t seq;
    int x;
    int y;
    int changed = 0;
    int mirror_ship_count = 0;

    seq = shared_read32(BATTLE_GRID_SEQ);

    if (!force && seq == last_battle_grid_seq)
        return;

    last_battle_grid_seq = seq;

    for (y = 0; y < BATTLE_H; y++)
    {
        for (x = 0; x < BATTLE_W; x++)
        {
            unsigned int index = (unsigned int)(y * BATTLE_W + x);
            uint8_t new_value = (shared_read32(BATTLE_GRID_BASE + index * 4) != 0)
                                ? BATTLE_CELL_SHIP
                                : BATTLE_CELL_EMPTY;

            if (new_value == BATTLE_CELL_SHIP)
                mirror_ship_count++;

            if (hidden_map[y][x] != new_value || force)
            {
                hidden_map[y][x] = new_value;

                if (new_value == BATTLE_CELL_EMPTY)
                    shot_map[y][x] = SHOT_NONE;

                mark_cell_dirty(x, y);
                changed = 1;
            }
        }
    }

    /*
       Python/IMG CLEAR writes an empty direct grid and bumps BATTLE_GRID_SEQ.
       Treat an empty grid as a full moderator reset: all ships removed,
       all shots cleared, popup cleared, and wait for new SHIP placements.
    */
    if (mirror_ship_count == 0)
    {
        if (layout_loaded || loaded_ship_cells != 0 || destroyed_ship_cells != 0 || force)
        {
            if (!force)
                trigger_sfx_flag(FLAG_SFX_CLEAR);
            clear_layout_and_round();
        }

        shared_write32(BATTLE_GRID_READY, 0);
        return;
    }

    if (changed || force)
    {
        fleet_popup_visible = 0;
        fleet_popup_drawn = 0;
        fleet_popup_dismissed = 0;
        recalc_ship_counts();
        hud_dirty = 1;
    }

    shared_write32(BATTLE_GRID_READY, 0);
}

/* =========================
   Mailbox from IMG decoder
   ========================= */
static void poll_battle_mailbox_once(void)
{
    uint32_t ready;
    uint32_t seq;
    uint32_t type;
    int x;
    int y;

    ready = shared_read32(BATTLE_MB_READY);
    if (ready == 0)
        return;

    seq = shared_read32(BATTLE_MB_SEQ);
    if (seq == last_battle_seq)
    {
        shared_write32(BATTLE_MB_ACK, seq);
        shared_write32(BATTLE_MB_READY, 0);
        return;
    }

    type = shared_read32(BATTLE_MB_TYPE);
    x = (int)shared_read32(BATTLE_MB_X);
    y = (int)shared_read32(BATTLE_MB_Y);

    if (type == BATTLE_CMD_CLEAR_ALL)
    {
        printf("[BATTLE MB] clear all\n");
        fflush(stdout);
        trigger_sfx_flag(FLAG_SFX_CLEAR);
        clear_layout_and_round();
    }
    else if (type == BATTLE_CMD_SET_SHIP)
    {
        if (in_bounds(x, y))
        {
            hidden_map[y][x] = BATTLE_CELL_SHIP;
            layout_loaded = 1;
            mark_cell_dirty(x, y);
            recalc_ship_counts();
            printf("[BATTLE MB] ship x=%d y=%d total=%d\n", x, y, loaded_ship_cells);
            fflush(stdout);
        }
    }
    else if (type == BATTLE_CMD_CLEAR_CELL)
    {
        if (in_bounds(x, y))
        {
            hidden_map[y][x] = BATTLE_CELL_EMPTY;
            shot_map[y][x] = SHOT_NONE;
            mark_cell_dirty(x, y);
            recalc_ship_counts();
            printf("[BATTLE MB] erase x=%d y=%d total=%d\n", x, y, loaded_ship_cells);
            fflush(stdout);
        }
    }

    last_battle_seq = seq;
    shared_write32(BATTLE_MB_ACK, seq);
    shared_write32(BATTLE_MB_READY, 0);
}

static void poll_battle_mailbox(void)
{
    /*
       Primary path: direct 10x10 hidden-map mirror. This avoids the old
       one-command mailbox stall during fast SHIP coordinate streaming.
    */
    sync_battle_grid_mirror(0);

    /*
       Backward-compatible path: drain any old mailbox commands if present.
    */
    {
        int i;
        for (i = 0; i < 4; i++)
        {
            if (shared_read32(BATTLE_MB_READY) == 0)
                break;
            poll_battle_mailbox_once();
        }
    }
}

/* =========================
   Cursor / reveal / controls
   ========================= */
static void update_cursor_from_accel(void)
{
    alt_32 x = 0;
    alt_32 y = 0;
    alt_32 z = 0;
    unsigned long now = tick_us();

    if (now - last_cursor_move_us < CURSOR_REPEAT_US)
        return;

    if (accel_read_x(&x) != 0 || accel_read_y(&y) != 0 || accel_read_z(&z) != 0)
        return;

    old_cursor_x = cursor_x;
    old_cursor_y = cursor_y;

    if (x > CURSOR_MOVE_THRESHOLD && cursor_x > 0)
    {
        /* Match Snake mapping: positive X moves left. */
        cursor_x--;
        last_cursor_move_us = now;
    }
    else if (x < -CURSOR_MOVE_THRESHOLD && cursor_x < BATTLE_W - 1)
    {
        /* Match Snake mapping: negative X moves right. */
        cursor_x++;
        last_cursor_move_us = now;
    }
    else if (y > CURSOR_MOVE_THRESHOLD && cursor_y < BATTLE_H - 1)
    {
        /* Match Snake mapping: positive Y moves down. */
        cursor_y++;
        last_cursor_move_us = now;
    }
    else if (y < -CURSOR_MOVE_THRESHOLD && cursor_y > 0)
    {
        /* Match Snake mapping: negative Y moves up. */
        cursor_y--;
        last_cursor_move_us = now;
    }

    if (old_cursor_x != cursor_x || old_cursor_y != cursor_y)
    {
        mark_cell_dirty(old_cursor_x, old_cursor_y);
        mark_cell_dirty(cursor_x, cursor_y);
        hud_dirty = 1;
    }
}

static void update_reveal_anim(void)
{
    unsigned long now = tick_us();
    int y;
    int col;

    if (!reveal_anim_active)
        return;

    if (now - last_reveal_step_us < REVEAL_SWEEP_STEP_US)
        return;

    last_reveal_step_us = now;

    col = reveal_columns;
    reveal_columns++;

    if (col >= 0 && col < BATTLE_W)
    {
        for (y = 0; y < BATTLE_H; y++)
            mark_cell_dirty(col, y);
    }

    if (reveal_columns >= BATTLE_W)
    {
        reveal_columns = BATTLE_W;
        reveal_anim_active = 0;
    }

    hud_dirty = 1;
}

static void toggle_moderator_reveal(void)
{
    if (!reveal_enabled || reveal_columns < BATTLE_W)
    {
        reveal_enabled = 1;
        reveal_anim_active = 0;
        reveal_columns = BATTLE_W;
        printf("[BATTLE] moderator reveal ON by KEY1\n");
    }
    else
    {
        reveal_enabled = 0;
        reveal_anim_active = 0;
        reveal_columns = 0;
        printf("[BATTLE] moderator reveal OFF by KEY1\n");
    }

    fflush(stdout);
    mark_all_cells_dirty();
    hud_dirty = 1;
}

static void choose_bomb_from_switches(uint32_t switch_state)
{
    int old_bomb = selected_bomb;

    if (switch_state & CONTROL_SW7_MASK)
        selected_bomb = BOMB_CROSS;
    else if (switch_state & CONTROL_SW6_MASK)
        selected_bomb = BOMB_SQUARE;
    else
        selected_bomb = BOMB_STANDARD;

    if (old_bomb != selected_bomb)
    {
        trigger_sfx_flag(FLAG_SFX_CHANGE_ARSENAL);
        publish_battle_status_to_control();
        hud_dirty = 1;
    }
}

void ship_game_handle_control_event(uint32_t key_mask,
                                    uint32_t switch_state,
                                    uint32_t event_type)
{
    uint32_t changed = switch_state ^ last_switch_state;
    uint32_t rising = changed & switch_state;

    (void)event_type;

    /*
       Fleet-down popup is modal.
       SW8 = continue viewing board.
       SW9 = global menu escape handled in main.c.
       KEY1 remains moderator reveal, but it does not dismiss the popup.
    */
    if (fleet_popup_visible)
    {
        if (key_mask != 0 || (rising & CONTROL_SW8_MASK) || (switch_state & CONTROL_SW8_MASK))
            trigger_sfx_flag(FLAG_SFX_CLICK);

        if ((rising & CONTROL_SW8_MASK) || (switch_state & CONTROL_SW8_MASK))
        {
            printf("[BATTLE] continue after fleet down by SW8\n");
            fflush(stdout);

            fleet_popup_visible = 0;
            fleet_popup_drawn = 0;
            fleet_popup_dismissed = 1;

            mark_all_cells_dirty();
            hud_dirty = 1;
            publish_battle_status_to_control();
        }

        /* Keys are locked while the end popup is visible. SW8 continues, SW9 exits in main.c. */

        last_switch_state = switch_state;
        return;
    }

    choose_bomb_from_switches(switch_state);

    if (key_mask & CONTROL_KEY0_MASK)
        fire_selected_bomb();

    if (key_mask & CONTROL_KEY1_MASK)
        toggle_moderator_reveal();

    /*
       SW8 is no longer reveal. KEY1 is the moderator reveal control.
       Keep SW8 reserved for fleet-down continue so it cannot accidentally
       expose the answer during normal gameplay.
    */

    last_switch_state = switch_state;
}

/* =========================
   Public API
   ========================= */
int ship_game_fleet_popup_visible(void)
{
    return fleet_popup_visible ? 1 : 0;
}

void ship_game_init(void)
{
    /* Always start the Battleship panel from a clean hidden layout.
       Previous runs could leave BATTLE_GRID_BASE cells in SDRAM, which made
       the board appear with default/stale ships before Python resent a fleet. */
    clear_battle_grid_mirror_memory();
    clear_layout_and_round();

    fake_time_us = 0;
    last_cursor_move_us = 0;
    last_reveal_step_us = 0;
    battle_rx_last_seq = shared_read32(FLAG_RT_ACTIVITY_SEQ);
    battle_rx_ticks = 0;
    battle_rx_indicator_active = 0;

    /* Clear any stale single-mailbox command from older decoder versions.
       New SHIP/CLEAR/DONE data from IMG will arrive after panel entry. */
    shared_write32(BATTLE_MB_READY, 0);

    full_redraw_needed = 1;
    hud_dirty = 1;
    board_dirty = 1;

    publish_battle_status_to_control();

    printf("[BATTLE] battleship screen init\n");
    fflush(stdout);

    redraw_dirty_cells_and_overlays();
}

void ship_game_update(void)
{
    /*
       Snake-like update model:
       - poll inputs/mailbox
       - mark only changed cells/HUD dirty
       - redraw only dirty cells + HUD, not the whole 320x240 screen
    */
    battle_update_rx_status();
    poll_battle_mailbox();

    if (!fleet_popup_visible)
    {
        update_cursor_from_accel();
        update_reveal_anim();
        update_blasts();
    }

    if (full_redraw_needed || board_dirty || hud_dirty || (fleet_popup_visible && !fleet_popup_drawn))
        redraw_dirty_cells_and_overlays();

    usleep(FRAME_DELAY_US);
}
