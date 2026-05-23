#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdint.h>

#include "io.h"
#include "system.h"
#include "decoder.h"
#include "shared_memory.h"

#define SNAKE_MB_READY             0x100
#define SNAKE_MB_ACK               0x104
#define SNAKE_MB_SEQ               0x108
#define SNAKE_MB_TYPE              0x10C
#define SNAKE_MB_COUNT             0x110
#define SNAKE_MB_FLAGS             0x114
#define SNAKE_MB_PAYLOAD_LEN       0x118
#define SNAKE_MB_STATUS            0x11C
#define SNAKE_MB_PAYLOAD           0x120
#define SNAKE_MB_PAYLOAD_MAX       256

#define SNAKE_CMD_NONE             0
#define SNAKE_CMD_SET_APPLE        1
#define SNAKE_CMD_SET_WALLS        2
#define SNAKE_CMD_ADD_WALLS        3
#define SNAKE_CMD_SET_PORTALS      4
#define SNAKE_CMD_CLEAR_ARENA      5
#define SNAKE_CMD_CLEAR_WALLS      6
#define SNAKE_CMD_ADD_APPLES       7
#define SNAKE_CMD_CLEAR_CELLS      8

#define SNAKE_GRID_W               32
#define SNAKE_GRID_H               22
#define MAILBOX_WAIT_RETRY         250
#define ROW_MAX                    16

#define SNAKE_STATE_NONE           0
#define SNAKE_STATE_WALL           1
#define SNAKE_STATE_APPLE          2
#define SNAKE_STATE_PORTA          3
#define SNAKE_STATE_PORTB          4
#define SNAKE_STATE_ERASE          5

#define BATTLE_STATE_NONE          0
#define BATTLE_STATE_SHIP          1
#define BATTLE_STATE_ERASE         2

#ifndef BATTLE_GRID_SEQ
#define BATTLE_GRID_SEQ                0xA40
#define BATTLE_GRID_READY              0xA44
#define BATTLE_GRID_BASE               0xA80
#endif

#ifndef DRAW_BG_READY
#define DRAW_BG_READY                  0x980
#define DRAW_BG_SEQ                    0x984
#define DRAW_BG_GRID                   0x1000
#endif

#ifndef FLAG_RT_ACTIVITY_SEQ
#define FLAG_RT_ACTIVITY_SEQ           0x870
#define FLAG_RT_PANEL_MODE             0x874
#define FLAG_RT_LAST_RESULT            0x878
#endif

static int snake_current_state = SNAKE_STATE_NONE;
static int snake_portal_a_valid = 0;
static int snake_portal_b_valid = 0;
static unsigned char snake_portal_ax = 0;
static unsigned char snake_portal_ay = 0;
static unsigned char snake_portal_bx = 0;
static unsigned char snake_portal_by = 0;

static int draw_current_color = 0xFF;
static unsigned int draw_grid_seq = 0;
static int draw_batch_mode = 0;
static int draw_grid_dirty = 0;
static int battle_current_state = BATTLE_STATE_NONE;

static void decoder_publish_realtime_activity(int panel_mode, int result)
{
    uint32_t seq = IORD_32DIRECT(SHARED_FLAGS_BASE, FLAG_RT_ACTIVITY_SEQ);
    IOWR_32DIRECT(SHARED_FLAGS_BASE, FLAG_RT_PANEL_MODE, (uint32_t)panel_mode);
    IOWR_32DIRECT(SHARED_FLAGS_BASE, FLAG_RT_LAST_RESULT, (uint32_t)result);
    IOWR_32DIRECT(SHARED_FLAGS_BASE, FLAG_RT_ACTIVITY_SEQ, seq + 1);
}

void decoder_reset_stream(void)
{
    snake_current_state = SNAKE_STATE_NONE;
    snake_portal_a_valid = 0;
    snake_portal_b_valid = 0;
    snake_portal_ax = 0;
    snake_portal_ay = 0;
    snake_portal_bx = 0;
    snake_portal_by = 0;
    draw_current_color = 0xFF;
    battle_current_state = BATTLE_STATE_NONE;
}

static void normalize_row(const char *text, unsigned int len, char *out, unsigned int out_cap)
{
    unsigned int i;
    unsigned int w = 0;

    if (out_cap == 0)
        return;

    for (i = 0; i < len && text[i] != '\0' && w < out_cap - 1; i++)
    {
        unsigned char c = (unsigned char)text[i];

        if (c == '\r' || c == '\n' || c == '\t' || c == ' ')
            continue;

        if (c >= 'a' && c <= 'z')
            c = (unsigned char)(c - 'a' + 'A');

        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            out[w++] = (char)c;
    }

    out[w] = '\0';
}

static int parse_two_digit(const char *s, int *value)
{
    if (s[0] < '0' || s[0] > '9') return 0;
    if (s[1] < '0' || s[1] > '9') return 0;
    *value = (s[0] - '0') * 10 + (s[1] - '0');
    return 1;
}

static int parse_xy_row(const char *row, int *x, int *y)
{
    if (row[0] != 'X')
        return 0;

    if (!parse_two_digit(&row[1], x))
        return 0;

    if (row[3] != 'Y' && row[3] != 'T')
        return 0;

    if (!parse_two_digit(&row[4], y))
        return 0;

    return 1;
}

static int snake_coord_valid(int x, int y)
{
    return (x >= 0 && x < SNAKE_GRID_W && y >= 0 && y < SNAKE_GRID_H);
}

static int draw_coord_valid(int x, int y)
{
    return (x >= 0 && x < DRAW_GRID_W && y >= 0 && y < DRAW_GRID_H);
}

static int battle_coord_valid(int x, int y)
{
    return (x >= 0 && x < BATTLE_GRID_W && y >= 0 && y < BATTLE_GRID_H);
}

static void snake_write_payload_word(unsigned int word_index, uint32_t value)
{
    IOWR_32DIRECT(SHARED_FLAGS_BASE, SNAKE_MB_PAYLOAD + (word_index * 4), value);
}

static int snake_send_mailbox_command_words(unsigned int cmd_type,
                                            unsigned int count,
                                            unsigned int word_count,
                                            const uint32_t *payload_words)
{
    static unsigned int seq = 0;
    unsigned int i;
    unsigned int wait_count = 0;
    unsigned int payload_len = word_count * 4;

    while (IORD_32DIRECT(SHARED_FLAGS_BASE, SNAKE_MB_READY) != 0)
    {
        usleep(1000);
        wait_count++;
        if (wait_count >= MAILBOX_WAIT_RETRY)
            return DECODER_ERR_MAILBOX_BUSY;
    }

    if (payload_len > SNAKE_MB_PAYLOAD_MAX)
        return DECODER_ERR_BAD_FORMAT;

    for (i = 0; payload_words != 0 && i < word_count; i++)
        snake_write_payload_word(i, payload_words[i]);

    seq++;
    IOWR_32DIRECT(SHARED_FLAGS_BASE, SNAKE_MB_TYPE, cmd_type);
    IOWR_32DIRECT(SHARED_FLAGS_BASE, SNAKE_MB_COUNT, count);
    IOWR_32DIRECT(SHARED_FLAGS_BASE, SNAKE_MB_PAYLOAD_LEN, payload_len);
    IOWR_32DIRECT(SHARED_FLAGS_BASE, SNAKE_MB_ACK, 0);
    IOWR_32DIRECT(SHARED_FLAGS_BASE, SNAKE_MB_SEQ, seq);
    IOWR_32DIRECT(SHARED_FLAGS_BASE, SNAKE_MB_READY, 1);
    return DECODER_OK_SENT;
}

static int snake_send_mailbox_command(unsigned int cmd_type,
                                      unsigned int count,
                                      unsigned int payload_len,
                                      const unsigned char *payload)
{
    (void)payload_len;
    (void)payload;
    return snake_send_mailbox_command_words(cmd_type, count, 0, 0);
}

static int snake_send_one_xy(unsigned int cmd, int x, int y)
{
    uint32_t payload_words[2];

    if (!snake_coord_valid(x, y))
        return DECODER_ERR_OUT_OF_RANGE;

    payload_words[0] = (uint32_t)x;
    payload_words[1] = (uint32_t)y;

    return snake_send_mailbox_command_words(cmd, 1, 2, payload_words);
}

static int snake_send_portals_if_ready(void)
{
    uint32_t payload_words[4];

    if (!snake_portal_a_valid || !snake_portal_b_valid)
        return DECODER_OK_WAITING_PORTAL;

    payload_words[0] = (uint32_t)snake_portal_ax;
    payload_words[1] = (uint32_t)snake_portal_ay;
    payload_words[2] = (uint32_t)snake_portal_bx;
    payload_words[3] = (uint32_t)snake_portal_by;

    return snake_send_mailbox_command_words(SNAKE_CMD_SET_PORTALS, 2, 4, payload_words);
}

static int decode_snake_row(const char *row)
{
    int x;
    int y;

    if (row[0] == '\0')
        return DECODER_NO_COMMAND;

    if (strcmp(row, "X99Y99") == 0 || strcmp(row, "X99T99") == 0)
        return DECODER_NO_COMMAND;

    if (strcmp(row, "WALL") == 0 || strcmp(row, "WALLS") == 0)
    {
        snake_current_state = SNAKE_STATE_WALL;
        return DECODER_OK_STATE_UPDATED;
    }
    if (strcmp(row, "APPLE") == 0 || strcmp(row, "APPLES") == 0)
    {
        snake_current_state = SNAKE_STATE_APPLE;
        return DECODER_OK_STATE_UPDATED;
    }
    if (strcmp(row, "PORTA") == 0 || strcmp(row, "PORTALA") == 0)
    {
        snake_current_state = SNAKE_STATE_PORTA;
        return DECODER_OK_STATE_UPDATED;
    }
    if (strcmp(row, "PORTB") == 0 || strcmp(row, "PORTALB") == 0)
    {
        snake_current_state = SNAKE_STATE_PORTB;
        return DECODER_OK_STATE_UPDATED;
    }
    if (strcmp(row, "ERASE") == 0 || strcmp(row, "EMPTY") == 0)
    {
        snake_current_state = SNAKE_STATE_ERASE;
        return DECODER_OK_STATE_UPDATED;
    }
    if (strcmp(row, "CLEAR") == 0 || strcmp(row, "RESET") == 0 ||
        strcmp(row, "CLR") == 0 || strcmp(row, "CLEARALL") == 0)
    {
        snake_current_state = SNAKE_STATE_NONE;
        snake_portal_a_valid = 0;
        snake_portal_b_valid = 0;
        return snake_send_mailbox_command(SNAKE_CMD_CLEAR_ARENA, 0, 0, 0);
    }
    if (strcmp(row, "CLRWAL") == 0 || strcmp(row, "NOWALL") == 0)
    {
        snake_current_state = SNAKE_STATE_NONE;
        return snake_send_mailbox_command(SNAKE_CMD_CLEAR_WALLS, 0, 0, 0);
    }

    if (!parse_xy_row(row, &x, &y))
        return DECODER_NO_COMMAND;

    if (snake_current_state == SNAKE_STATE_WALL)
        return snake_send_one_xy(SNAKE_CMD_ADD_WALLS, x, y);
    if (snake_current_state == SNAKE_STATE_APPLE)
        return snake_send_one_xy(SNAKE_CMD_ADD_APPLES, x, y);
    if (snake_current_state == SNAKE_STATE_ERASE)
        return snake_send_one_xy(SNAKE_CMD_CLEAR_CELLS, x, y);
    if (snake_current_state == SNAKE_STATE_PORTA)
    {
        if (!snake_coord_valid(x, y))
            return DECODER_ERR_OUT_OF_RANGE;
        snake_portal_ax = (unsigned char)x;
        snake_portal_ay = (unsigned char)y;
        snake_portal_a_valid = 1;
        return snake_send_portals_if_ready();
    }
    if (snake_current_state == SNAKE_STATE_PORTB)
    {
        if (!snake_coord_valid(x, y))
            return DECODER_ERR_OUT_OF_RANGE;
        snake_portal_bx = (unsigned char)x;
        snake_portal_by = (unsigned char)y;
        snake_portal_b_valid = 1;
        return snake_send_portals_if_ready();
    }

    return DECODER_NO_COMMAND;
}

static int draw_color_from_row(const char *row, int *out_color)
{
    /*
       Draw panel now uses full 8-bit RGB332 values.
    */
    if (strcmp(row, "BLACK") == 0 || strcmp(row, "BLK") == 0) { *out_color = 0x00; return 1; }
    if (strcmp(row, "BLUE") == 0 || strcmp(row, "BLU") == 0) { *out_color = 0x03; return 1; }
    if (strcmp(row, "GREEN") == 0 || strcmp(row, "GRN") == 0) { *out_color = 0x1C; return 1; }
    if (strcmp(row, "CYAN") == 0) { *out_color = 0x1F; return 1; }
    if (strcmp(row, "RED") == 0) { *out_color = 0xE0; return 1; }
    if (strcmp(row, "PURPLE") == 0 || strcmp(row, "MAGENT") == 0 || strcmp(row, "PINK") == 0) { *out_color = 0xE3; return 1; }
    if (strcmp(row, "YELLOW") == 0 || strcmp(row, "YELL") == 0) { *out_color = 0xFC; return 1; }
    if (strcmp(row, "WHITE") == 0 || strcmp(row, "WHT") == 0) { *out_color = 0xFF; return 1; }
    return 0;
}

static void draw_publish_grid_if_dirty(void)
{
    if (!draw_grid_dirty)
        return;

    draw_grid_seq++;
    IOWR_32DIRECT(SHARED_FLAGS_BASE, DRAW_BG_SEQ, draw_grid_seq);
    IOWR_32DIRECT(SHARED_FLAGS_BASE, DRAW_BG_READY, 1);
    draw_grid_dirty = 0;
}

static void draw_grid_write_pixel(int x, int y, int color)
{
    unsigned int index;

    if (!draw_coord_valid(x, y))
        return;

    index = (unsigned int)(y * DRAW_GRID_W + x);
    IOWR_8DIRECT(SHARED_FLAGS_BASE, DRAW_BG_GRID + index, (uint8_t)(color & 0xFF));
    draw_grid_dirty = 1;
}

static void draw_grid_clear_all(void)
{
    unsigned int i;

    for (i = 0; i < (unsigned int)(DRAW_GRID_W * DRAW_GRID_H); i++)
        IOWR_8DIRECT(SHARED_FLAGS_BASE, DRAW_BG_GRID + i, 0);

    draw_grid_dirty = 1;

    if (!draw_batch_mode)
        draw_publish_grid_if_dirty();
}

static int draw_send_pixel(int x, int y, int color)
{
    if (!draw_coord_valid(x, y))
        return DECODER_ERR_OUT_OF_RANGE;

    draw_grid_write_pixel(x, y, color);

    if (!draw_batch_mode)
        draw_publish_grid_if_dirty();

    return DECODER_OK_SENT;
}

static int decode_draw_row(const char *row)
{
    int x;
    int y;
    int color;

    if (row[0] == '\0')
        return DECODER_NO_COMMAND;
    if (strcmp(row, "X99Y99") == 0 || strcmp(row, "X99T99") == 0)
        return DECODER_NO_COMMAND;

    if (draw_color_from_row(row, &color))
    {
        draw_current_color = color;
        return DECODER_OK_STATE_UPDATED;
    }

    if (strcmp(row, "CLEAR") == 0 || strcmp(row, "RESET") == 0 ||
        strcmp(row, "CLR") == 0 || strcmp(row, "CLEARALL") == 0)
    {
        draw_current_color = 0x00;
        draw_grid_clear_all();
        return DECODER_OK_SENT;
    }

    if (strcmp(row, "ERASE") == 0 || strcmp(row, "EMPTY") == 0)
    {
        draw_current_color = 0x00;
        return DECODER_OK_STATE_UPDATED;
    }

    if (!parse_xy_row(row, &x, &y))
        return DECODER_NO_COMMAND;

    return draw_send_pixel(x, y, draw_current_color);
}

static void battle_publish_grid_seq(void)
{
    static unsigned int seq = 0;

    seq++;
    IOWR_32DIRECT(SHARED_FLAGS_BASE, BATTLE_GRID_SEQ, seq);
    IOWR_32DIRECT(SHARED_FLAGS_BASE, BATTLE_GRID_READY, 1);
}

static void battle_grid_write_cell(int x, int y, uint32_t value)
{
    unsigned int index;

    if (!battle_coord_valid(x, y))
        return;

    index = (unsigned int)(y * BATTLE_GRID_W + x);
    IOWR_32DIRECT(SHARED_FLAGS_BASE, BATTLE_GRID_BASE + index * 4, value);
}

static void battle_grid_clear_all(void)
{
    unsigned int i;

    for (i = 0; i < (unsigned int)(BATTLE_GRID_W * BATTLE_GRID_H); i++)
        IOWR_32DIRECT(SHARED_FLAGS_BASE, BATTLE_GRID_BASE + i * 4, 0);
}

static int battle_send_command(unsigned int cmd_type, int x, int y)
{
    /*
       Battleship originally used a single ready/ack mailbox. During fast
       Python layout streaming that can stall after several SHIP coordinates
       if VGA is busy rendering. Use a direct 10x10 hidden-map mirror instead:
       IMG writes the cell word, bumps BATTLE_GRID_SEQ, and VGA syncs the map
       in its normal update loop.
    */

    if (cmd_type == BATTLE_CMD_CLEAR_ALL)
    {
        battle_grid_clear_all();
        battle_publish_grid_seq();
        return DECODER_OK_SENT;
    }

    if (!battle_coord_valid(x, y))
        return DECODER_ERR_OUT_OF_RANGE;

    if (cmd_type == BATTLE_CMD_SET_SHIP)
    {
        battle_grid_write_cell(x, y, 1);
        battle_publish_grid_seq();
        return DECODER_OK_SENT;
    }

    if (cmd_type == BATTLE_CMD_CLEAR_CELL)
    {
        battle_grid_write_cell(x, y, 0);
        battle_publish_grid_seq();
        return DECODER_OK_SENT;
    }

    return DECODER_ERR_BAD_FORMAT;
}

static int decode_battle_row(const char *row)
{
    int x;
    int y;

    if (row[0] == '\0')
        return DECODER_NO_COMMAND;
    if (strcmp(row, "X99Y99") == 0 || strcmp(row, "X99T99") == 0)
        return DECODER_NO_COMMAND;

    if (strcmp(row, "SHIP") == 0 || strcmp(row, "SHIPS") == 0)
    {
        battle_current_state = BATTLE_STATE_SHIP;
        return DECODER_OK_STATE_UPDATED;
    }
    if (strcmp(row, "ERASE") == 0 || strcmp(row, "EMPTY") == 0 || strcmp(row, "WATER") == 0)
    {
        battle_current_state = BATTLE_STATE_ERASE;
        return DECODER_OK_STATE_UPDATED;
    }
    if (strcmp(row, "CLEAR") == 0 || strcmp(row, "RESET") == 0 || strcmp(row, "CLR") == 0 || strcmp(row, "CLEARALL") == 0)
    {
        battle_current_state = BATTLE_STATE_NONE;
        return battle_send_command(BATTLE_CMD_CLEAR_ALL, 0, 0);
    }
    if (strcmp(row, "DONE") == 0)
    {
        return DECODER_OK_STATE_UPDATED;
    }

    if (!parse_xy_row(row, &x, &y))
        return DECODER_NO_COMMAND;
    if (!battle_coord_valid(x, y))
        return DECODER_ERR_OUT_OF_RANGE;

    if (battle_current_state == BATTLE_STATE_SHIP)
        return battle_send_command(BATTLE_CMD_SET_SHIP, x, y);
    if (battle_current_state == BATTLE_STATE_ERASE)
        return battle_send_command(BATTLE_CMD_CLEAR_CELL, x, y);

    return DECODER_NO_COMMAND;
}


static void decoder_trim_row(char *row)
{
    int start = 0;
    int end;
    int i;

    while (row[start] == ' ' || row[start] == '\t')
        start++;

    if (start > 0)
    {
        i = 0;
        while (row[start] != '\0')
            row[i++] = row[start++];
        row[i] = '\0';
    }

    end = (int)strlen(row) - 1;
    while (end >= 0 && (row[end] == ' ' || row[end] == '\t' ||
                        row[end] == '\r' || row[end] == '\n'))
    {
        row[end] = '\0';
        end--;
    }
}

int decoder_decode_and_store_panel_text_batch(int panel_mode, const char *text, unsigned int len)
{
    char row[ROW_MAX];
    unsigned int i;
    unsigned int row_len = 0;
    int result = DECODER_NO_COMMAND;
    int last_result = DECODER_NO_COMMAND;
    int row_count = 0;

    if (text == 0 || len == 0)
        return DECODER_NO_COMMAND;

    if (panel_mode == DECODER_PANEL_DEBUG || panel_mode == GAME_MODE_DEBUG)
        return decoder_decode_and_store_panel_text_len(panel_mode, text, len);

    if (panel_mode == DECODER_PANEL_DRAW || panel_mode == GAME_MODE_DRAW)
        draw_batch_mode = 1;

    for (i = 0; i <= len; i++)
    {
        char c = (i < len) ? text[i] : ';';

        if (c == ';' || c == '\n' || c == '\r' || c == '\0')
        {
            row[row_len] = '\0';
            decoder_trim_row(row);

            if (row[0] != '\0')
            {
                result = decoder_decode_and_store_panel_text_len(panel_mode, row, (unsigned int)strlen(row));
                last_result = result;
                row_count++;

                if (result == DECODER_ERR_MAILBOX_BUSY &&
                    !(panel_mode == DECODER_PANEL_DRAW || panel_mode == GAME_MODE_DRAW))
                {
                    break;
                }
            }

            row_len = 0;

            if (c == '\0')
                break;
        }
        else if (row_len < (ROW_MAX - 1))
        {
            row[row_len++] = c;
        }
    }

    if (panel_mode == DECODER_PANEL_DRAW || panel_mode == GAME_MODE_DRAW)
    {
        draw_batch_mode = 0;
        draw_publish_grid_if_dirty();

        if (row_count > 1 && last_result >= 0)
            return DECODER_OK_BATCH_SENT;
    }

    return last_result;
}


int decoder_decode_and_store_panel_text_len(int panel_mode, const char *text, unsigned int len)
{
    char row[ROW_MAX];
    int result;

    if (text == 0 || len == 0)
        return DECODER_NO_COMMAND;

    normalize_row(text, len, row, sizeof(row));

    if (panel_mode == DECODER_PANEL_DEBUG || panel_mode == GAME_MODE_DEBUG)
        return DECODER_NO_COMMAND;

    if (panel_mode == DECODER_PANEL_SNAKE || panel_mode == GAME_MODE_SNAKE)
    {
        result = decode_snake_row(row);
        if (row[0] != '\0')
            decoder_publish_realtime_activity(GAME_MODE_SNAKE, result);
        return result;
    }

    if (panel_mode == DECODER_PANEL_DRAW || panel_mode == GAME_MODE_DRAW)
    {
        result = decode_draw_row(row);
        if (row[0] != '\0')
            decoder_publish_realtime_activity(GAME_MODE_DRAW, result);
        return result;
    }

    if (panel_mode == DECODER_PANEL_BATTLE || panel_mode == GAME_MODE_BATTLE)
    {
        result = decode_battle_row(row);
        if (row[0] != '\0')
            decoder_publish_realtime_activity(GAME_MODE_BATTLE, result);
        return result;
    }

    return DECODER_ERR_BAD_PANEL;
}

int decoder_decode_and_store_panel_text(int panel_mode, const char *text)
{
    if (text == 0)
        return DECODER_NO_COMMAND;

    return decoder_decode_and_store_panel_text_batch(panel_mode, text, (unsigned int)strlen(text));
}

int decoder_decode_and_store_snake_command_len(const char *text, unsigned int len)
{
    return decoder_decode_and_store_panel_text_len(GAME_MODE_SNAKE, text, len);
}

int decoder_decode_and_store_snake_command(const char *text)
{
    return decoder_decode_and_store_panel_text(GAME_MODE_SNAKE, text);
}
