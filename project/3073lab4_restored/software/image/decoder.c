#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "io.h"
#include "system.h"
#include "decoder.h"

/* ============================================================
   SNAKE SDRAM MAILBOX
   ============================================================ */

#define SHARED_FLAGS_BASE          0x05212000

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

/* ============================================================
   COMMAND TYPES
   ============================================================ */

#define SNAKE_CMD_NONE             0
#define SNAKE_CMD_SET_APPLE        1
#define SNAKE_CMD_SET_WALLS        2
#define SNAKE_CMD_ADD_WALLS        3
#define SNAKE_CMD_SET_PORTALS      4
#define SNAKE_CMD_CLEAR_ARENA      5
#define SNAKE_CMD_CLEAR_WALLS      6

/* ============================================================
   GRID LIMITS
   ============================================================ */

#define SNAKE_GRID_W               32
#define SNAKE_GRID_H               22

/* ============================================================
   STREAM SETTINGS
   ============================================================ */

#define DECODER_STREAM_MAX         512
#define MAILBOX_WAIT_RETRY         1000

/* ============================================================
   STREAM BUFFER
   ============================================================ */

static char stream_buf[DECODER_STREAM_MAX];
static unsigned int stream_len = 0;

/* ============================================================
   PORTAL LOCAL STATE
   ============================================================ */

static int portal_a_valid = 0;
static int portal_b_valid = 0;

static unsigned char portal_ax = 0;
static unsigned char portal_ay = 0;
static unsigned char portal_bx = 0;
static unsigned char portal_by = 0;

/* ============================================================
   BASIC HELPERS
   ============================================================ */

void decoder_reset_stream(void)
{
    stream_len = 0;
    stream_buf[0] = '\0';

    portal_a_valid = 0;
    portal_b_valid = 0;
}

static int coord_valid(int x, int y)
{
    if (x < 0 || x >= SNAKE_GRID_W) return 0;
    if (y < 0 || y >= SNAKE_GRID_H) return 0;
    return 1;
}

static void snake_write_payload_byte(unsigned int index, unsigned char value)
{
    volatile unsigned char *payload;

    payload = (volatile unsigned char *)(SHARED_FLAGS_BASE + SNAKE_MB_PAYLOAD);
    payload[index] = value;
}

static int snake_send_mailbox_command(
    unsigned int cmd_type,
    unsigned int count,
    unsigned int payload_len,
    const unsigned char *payload
)
{
    static unsigned int seq = 0;
    unsigned int i;
    unsigned int wait_count = 0;

    while (IORD_32DIRECT(SHARED_FLAGS_BASE, SNAKE_MB_READY) != 0)
    {
        usleep(1000);
        wait_count++;

        if (wait_count >= MAILBOX_WAIT_RETRY)
        {
            return DECODER_ERR_MAILBOX_BUSY;
        }
    }

    if (payload_len > SNAKE_MB_PAYLOAD_MAX)
    {
        payload_len = SNAKE_MB_PAYLOAD_MAX;
    }

    for (i = 0; i < payload_len; i++)
    {
        snake_write_payload_byte(i, payload[i]);
    }

    seq++;

    IOWR_32DIRECT(SHARED_FLAGS_BASE, SNAKE_MB_TYPE, cmd_type);
    IOWR_32DIRECT(SHARED_FLAGS_BASE, SNAKE_MB_COUNT, count);
    IOWR_32DIRECT(SHARED_FLAGS_BASE, SNAKE_MB_PAYLOAD_LEN, payload_len);
    IOWR_32DIRECT(SHARED_FLAGS_BASE, SNAKE_MB_ACK, 0);
    IOWR_32DIRECT(SHARED_FLAGS_BASE, SNAKE_MB_SEQ, seq);

    /*
       READY must be written last.
    */
    IOWR_32DIRECT(SHARED_FLAGS_BASE, SNAKE_MB_READY, 1);

    return DECODER_OK_SENT;
}

static int parse_number_range(
    const char *buf,
    unsigned int *pos,
    unsigned int end,
    int *out_value
)
{
    int value = 0;
    int found_digit = 0;

    while (*pos < end && buf[*pos] >= '0' && buf[*pos] <= '9')
    {
        found_digit = 1;
        value = value * 10 + (buf[*pos] - '0');
        (*pos)++;
    }

    if (!found_digit)
    {
        return 0;
    }

    *out_value = value;
    return 1;
}

/*
   Parse XnYn pairs from buf[start..end).

   If the last pair is incomplete because the next SPI chunk has not arrived,
   this returns success with only completed pairs.
*/
static int parse_xy_pairs_range(
    const char *buf,
    unsigned int start,
    unsigned int end,
    unsigned char *payload,
    unsigned int max_payload_len,
    unsigned int *out_count,
    unsigned int *out_payload_len
)
{
    unsigned int pos = start;
    unsigned int count = 0;
    unsigned int payload_len = 0;

    while (pos < end)
    {
        unsigned int pair_start = pos;
        int x;
        int y;

        if (buf[pos] != 'X')
        {
            if ((end - pos) >= 5 && strncmp(&buf[pos], "SNAKE", 5) == 0)
            {
                break;
            }

            return DECODER_ERR_BAD_FORMAT;
        }

        pos++;

        if (!parse_number_range(buf, &pos, end, &x))
        {
            break;
        }

        if (pos >= end)
        {
            pos = pair_start;
            break;
        }

        if (buf[pos] != 'Y')
        {
            return DECODER_ERR_BAD_FORMAT;
        }

        pos++;

        if (!parse_number_range(buf, &pos, end, &y))
        {
            pos = pair_start;
            break;
        }

        if (!coord_valid(x, y))
        {
            return DECODER_ERR_OUT_OF_RANGE;
        }

        if (payload_len + 2 > max_payload_len)
        {
            break;
        }

        payload[payload_len++] = (unsigned char)x;
        payload[payload_len++] = (unsigned char)y;
        count++;
    }

    *out_count = count;
    *out_payload_len = payload_len;

    return DECODER_OK_SENT;
}

/* ============================================================
   STREAM HELPERS
   ============================================================ */

static int starts_with_at(const char *buf, unsigned int pos, const char *prefix)
{
    unsigned int prefix_len;

    prefix_len = (unsigned int)strlen(prefix);

    if (pos + prefix_len > stream_len)
    {
        return 0;
    }

    return strncmp(&buf[pos], prefix, prefix_len) == 0;
}

static int find_token_from(unsigned int start, const char *token)
{
    unsigned int i;
    unsigned int token_len;

    token_len = (unsigned int)strlen(token);

    if (token_len == 0 || stream_len < token_len)
    {
        return -1;
    }

    for (i = start; i + token_len <= stream_len; i++)
    {
        if (strncmp(&stream_buf[i], token, token_len) == 0)
        {
            return (int)i;
        }
    }

    return -1;
}

static int find_next_snake(unsigned int start)
{
    return find_token_from(start, "SNAKE");
}

static void remove_stream_prefix(unsigned int count)
{
    unsigned int i;

    if (count == 0)
    {
        return;
    }

    if (count >= stream_len)
    {
        stream_len = 0;
        stream_buf[0] = '\0';
        return;
    }

    for (i = 0; i < stream_len - count; i++)
    {
        stream_buf[i] = stream_buf[i + count];
    }

    stream_len -= count;
    stream_buf[stream_len] = '\0';
}

/*
   Camera sometimes reads:
       S AKEAPP
   instead of:
       SNAKEAPP

   This repair inserts missing N in SAKE...
*/
static void repair_common_ocr_errors(void)
{
    unsigned int i;

    i = 0;

    while (i + 4 <= stream_len)
    {
        if (strncmp(&stream_buf[i], "SAKE", 4) == 0)
        {
            unsigned int j;

            if (stream_len + 1 < DECODER_STREAM_MAX)
            {
                for (j = stream_len + 1; j > i + 1; j--)
                {
                    stream_buf[j] = stream_buf[j - 1];
                }

                stream_buf[i + 1] = 'N';
                stream_len++;
                stream_buf[stream_len] = '\0';

                i += 5;
                continue;
            }
        }

        i++;
    }
}

static void append_normalized_chunk(const char *text, unsigned int len)
{
    unsigned int i;

    for (i = 0; i < len; i++)
    {
        unsigned char c;

        c = (unsigned char)text[i];

        if (c == '\0')
        {
            break;
        }

        /*
           Keep only letters and numbers.
           Removes spaces/newlines from fixed-column output.
        */
        if (isalnum(c))
        {
            if (stream_len + 1 >= DECODER_STREAM_MAX)
            {
                int first_snake;

                first_snake = find_next_snake(0);

                if (first_snake > 0)
                {
                    remove_stream_prefix((unsigned int)first_snake);
                }
                else
                {
                    stream_len = 0;
                    stream_buf[0] = '\0';
                }
            }

            if (stream_len + 1 < DECODER_STREAM_MAX)
            {
                stream_buf[stream_len++] = (char)toupper(c);
                stream_buf[stream_len] = '\0';
            }
        }
    }

    repair_common_ocr_errors();
}

static void drop_junk_before_command(void)
{
    int first_snake;
    int first_sake;
    int first;

    first_snake = find_token_from(0, "SNAKE");
    first_sake = find_token_from(0, "SAKE");

    if (first_snake < 0 && first_sake < 0)
    {
        /*
           Keep a small suffix in case next chunk completes SNAKE.
        */
        if (stream_len > 8)
        {
            remove_stream_prefix(stream_len - 8);
        }
        return;
    }

    if (first_snake < 0)
    {
        first = first_sake;
    }
    else if (first_sake < 0)
    {
        first = first_snake;
    }
    else
    {
        first = (first_snake < first_sake) ? first_snake : first_sake;
    }

    if (first > 0)
    {
        remove_stream_prefix((unsigned int)first);
    }

    repair_common_ocr_errors();
}

/* ============================================================
   COMMAND SEND HELPERS
   ============================================================ */

static int send_apple_from_segment(unsigned int data_start, unsigned int data_end)
{
    unsigned char payload[2];
    unsigned int count;
    unsigned int payload_len;
    int result;

    result = parse_xy_pairs_range(
        stream_buf,
        data_start,
        data_end,
        payload,
        sizeof(payload),
        &count,
        &payload_len
    );

    if (result < 0)
    {
        return result;
    }

    if (count < 1 || payload_len < 2)
    {
        return DECODER_NO_COMMAND;
    }

    /*
       Snake mailbox expects one apple.
       If multiple apples appear, use the first one.
    */
    return snake_send_mailbox_command(
        SNAKE_CMD_SET_APPLE,
        1,
        2,
        payload
    );
}

static int send_walls_from_segment(unsigned int data_start, unsigned int data_end)
{
    unsigned char payload[SNAKE_MB_PAYLOAD_MAX];
    unsigned int count;
    unsigned int payload_len;
    int result;

    result = parse_xy_pairs_range(
        stream_buf,
        data_start,
        data_end,
        payload,
        sizeof(payload),
        &count,
        &payload_len
    );

    if (result < 0)
    {
        return result;
    }

    if (count == 0 || payload_len == 0)
    {
        return DECODER_NO_COMMAND;
    }

    /*
       IMPORTANT:
       No duplicate/similar ignoring here.

       Similar-looking X/Y chunks are valid for snake:
           X5Y16X6
           Y16X8Y16
           X10Y16X1
           2Y16X14Y
           16X16Y16
    */
    return snake_send_mailbox_command(
        SNAKE_CMD_SET_WALLS,
        count,
        payload_len,
        payload
    );
}

static int send_add_walls_from_segment(unsigned int data_start, unsigned int data_end)
{
    unsigned char payload[SNAKE_MB_PAYLOAD_MAX];
    unsigned int count;
    unsigned int payload_len;
    int result;

    result = parse_xy_pairs_range(
        stream_buf,
        data_start,
        data_end,
        payload,
        sizeof(payload),
        &count,
        &payload_len
    );

    if (result < 0)
    {
        return result;
    }

    if (count == 0 || payload_len == 0)
    {
        return DECODER_NO_COMMAND;
    }

    return snake_send_mailbox_command(
        SNAKE_CMD_ADD_WALLS,
        count,
        payload_len,
        payload
    );
}

static int send_portal_a_from_segment(unsigned int data_start, unsigned int data_end)
{
    unsigned char payload[2];
    unsigned int count;
    unsigned int payload_len;
    int result;

    result = parse_xy_pairs_range(
        stream_buf,
        data_start,
        data_end,
        payload,
        sizeof(payload),
        &count,
        &payload_len
    );

    if (result < 0)
    {
        return result;
    }

    if (count < 1 || payload_len < 2)
    {
        return DECODER_NO_COMMAND;
    }

    portal_ax = payload[0];
    portal_ay = payload[1];
    portal_a_valid = 1;

    if (portal_b_valid)
    {
        unsigned char portal_payload[4];

        portal_payload[0] = portal_ax;
        portal_payload[1] = portal_ay;
        portal_payload[2] = portal_bx;
        portal_payload[3] = portal_by;

        return snake_send_mailbox_command(
            SNAKE_CMD_SET_PORTALS,
            2,
            4,
            portal_payload
        );
    }

    return DECODER_OK_WAITING_PORTAL;
}

static int send_portal_b_from_segment(unsigned int data_start, unsigned int data_end)
{
    unsigned char payload[2];
    unsigned int count;
    unsigned int payload_len;
    int result;

    result = parse_xy_pairs_range(
        stream_buf,
        data_start,
        data_end,
        payload,
        sizeof(payload),
        &count,
        &payload_len
    );

    if (result < 0)
    {
        return result;
    }

    if (count < 1 || payload_len < 2)
    {
        return DECODER_NO_COMMAND;
    }

    portal_bx = payload[0];
    portal_by = payload[1];
    portal_b_valid = 1;

    if (portal_a_valid)
    {
        unsigned char portal_payload[4];

        portal_payload[0] = portal_ax;
        portal_payload[1] = portal_ay;
        portal_payload[2] = portal_bx;
        portal_payload[3] = portal_by;

        return snake_send_mailbox_command(
            SNAKE_CMD_SET_PORTALS,
            2,
            4,
            portal_payload
        );
    }

    return DECODER_OK_WAITING_PORTAL;
}

static int send_clear_arena(void)
{
    return snake_send_mailbox_command(
        SNAKE_CMD_CLEAR_ARENA,
        0,
        0,
        0
    );
}

static int send_clear_walls(void)
{
    return snake_send_mailbox_command(
        SNAKE_CMD_CLEAR_WALLS,
        0,
        0,
        0
    );
}

/* ============================================================
   STREAM PROCESSOR
   ============================================================ */

static int process_stream(void)
{
    int any_sent;
    int last_result;

    any_sent = 0;
    last_result = DECODER_NO_COMMAND;

    while (stream_len > 0)
    {
        int next_snake;
        unsigned int cmd_end;
        unsigned int remove_after_send;
        int result;

        drop_junk_before_command();

        if (stream_len < 5)
        {
            break;
        }

        if (strncmp(stream_buf, "SNAKE", 5) != 0)
        {
            break;
        }

        next_snake = find_next_snake(5);

        if (next_snake > 0)
        {
            cmd_end = (unsigned int)next_snake;
        }
        else
        {
            cmd_end = stream_len;
        }

        result = DECODER_NO_COMMAND;
        remove_after_send = 0;

        if (starts_with_at(stream_buf, 0, "SNAKEAPPLE"))
        {
            result = send_apple_from_segment(
                (unsigned int)strlen("SNAKEAPPLE"),
                cmd_end
            );

            if (result == DECODER_OK_SENT)
            {
                remove_after_send = cmd_end;
            }
        }
        else if (starts_with_at(stream_buf, 0, "SNAKEADDWALL"))
        {
            result = send_add_walls_from_segment(
                (unsigned int)strlen("SNAKEADDWALL"),
                cmd_end
            );

            if (next_snake > 0 && result >= 0)
            {
                remove_after_send = cmd_end;
            }
        }
        else if (starts_with_at(stream_buf, 0, "SNAKEWALL"))
        {
            result = send_walls_from_segment(
                (unsigned int)strlen("SNAKEWALL"),
                cmd_end
            );

            /*
               If another SNAKE command has started, remove this command.
               If no next SNAKE yet, keep it so later chunks can extend it.
            */
            if (next_snake > 0 && result >= 0)
            {
                remove_after_send = cmd_end;
            }
        }
        else if (starts_with_at(stream_buf, 0, "SNAKEPORTALA"))
        {
            result = send_portal_a_from_segment(
                (unsigned int)strlen("SNAKEPORTALA"),
                cmd_end
            );

            if (result == DECODER_OK_SENT || result == DECODER_OK_WAITING_PORTAL)
            {
                remove_after_send = cmd_end;
            }
        }
        else if (starts_with_at(stream_buf, 0, "SNAKEPORTALB"))
        {
            result = send_portal_b_from_segment(
                (unsigned int)strlen("SNAKEPORTALB"),
                cmd_end
            );

            if (result == DECODER_OK_SENT || result == DECODER_OK_WAITING_PORTAL)
            {
                remove_after_send = cmd_end;
            }
        }
        else if (starts_with_at(stream_buf, 0, "SNAKECLEARARENA"))
        {
            result = send_clear_arena();

            if (result == DECODER_OK_SENT)
            {
                remove_after_send = (unsigned int)strlen("SNAKECLEARARENA");
            }
        }
        else if (starts_with_at(stream_buf, 0, "SNAKECLEARWALLS"))
        {
            result = send_clear_walls();

            if (result == DECODER_OK_SENT)
            {
                remove_after_send = (unsigned int)strlen("SNAKECLEARWALLS");
            }
        }
        else if (starts_with_at(stream_buf, 0, "SNAKEEMPTY"))
        {
            remove_after_send = (unsigned int)strlen("SNAKEEMPTY");
            result = DECODER_NO_COMMAND;
        }
        else
        {
            /*
               Example:
                   SNAKEWAL
                   SNAKEAPP
               Not enough chars yet.
            */
            break;
        }

        if (result < 0)
        {
            return result;
        }

        if (result == DECODER_OK_SENT || result == DECODER_OK_WAITING_PORTAL)
        {
            any_sent = 1;
            last_result = result;
        }

        if (remove_after_send > 0)
        {
            remove_stream_prefix(remove_after_send);
            continue;
        }

        /*
           No removal means wait for more chunks.
        */
        break;
    }

    if (any_sent)
    {
        return last_result;
    }

    return DECODER_NO_COMMAND;
}

/* ============================================================
   PUBLIC API
   ============================================================ */

int decoder_decode_and_store_snake_command_len(const char *text, unsigned int len)
{
    if (text == 0 || len == 0)
    {
        return DECODER_NO_COMMAND;
    }

    append_normalized_chunk(text, len);

    return process_stream();
}

int decoder_decode_and_store_snake_command(const char *text)
{
    if (text == 0)
    {
        return DECODER_NO_COMMAND;
    }

    return decoder_decode_and_store_snake_command_len(
        text,
        (unsigned int)strlen(text)
    );
}
