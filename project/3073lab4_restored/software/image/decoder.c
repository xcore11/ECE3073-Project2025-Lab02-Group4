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
   These must match snake.c / VGA mailbox command decode.
   ============================================================ */

#define SNAKE_CMD_NONE             0
#define SNAKE_CMD_SET_APPLE        1
#define SNAKE_CMD_SET_WALLS        2
#define SNAKE_CMD_ADD_WALLS        3
#define SNAKE_CMD_SET_PORTALS      4
#define SNAKE_CMD_CLEAR_ARENA      5
#define SNAKE_CMD_CLEAR_WALLS      6
#define SNAKE_CMD_ADD_APPLES       7

/*
   WALL + PORTAL ONLY BUILD:
   Apples are intentionally ignored in this decoder version. The apple command
   parser is left out of the active stream path so apple rows do not affect wall
   or portal updates.
*/

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
/*
   Set by parse_xy_pairs_range() when at least one valid coordinate was parsed,
   then non-coordinate trailing junk was found. That means the current text
   command should be considered complete and removed from the stream, otherwise
   a later wall command can be absorbed into an old apple command or vice versa.

   NOTE:
   Do NOT suppress exact duplicate mailbox commands here. The same arena command
   may legitimately be sent again on the next scroll/pass. Suppressing duplicates
   was what made the first try work but the second try appear to do nothing.
*/
static int parse_stopped_on_trailing_junk = 0;

/* ============================================================
   PORTAL LOCAL STATE

   Portal command can arrive in either form:

       SNAKEPORTALAX8Y3
       SNAKEPORTALBX21Y5

   or compact form:

       SNAKEPORTALAX8Y3X21Y5

   In compact form, the first XY pair is portal A and the second XY pair is
   portal B. The decoder sends SNAKE_CMD_SET_PORTALS only when both A and B
   are known.
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

    parse_stopped_on_trailing_junk = 0;
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

    if (payload != 0)
    {
        for (i = 0; i < payload_len; i++)
        {
            snake_write_payload_byte(i, payload[i]);
        }
    }

    seq++;

    IOWR_32DIRECT(SHARED_FLAGS_BASE, SNAKE_MB_TYPE, cmd_type);
    IOWR_32DIRECT(SHARED_FLAGS_BASE, SNAKE_MB_COUNT, count);
    IOWR_32DIRECT(SHARED_FLAGS_BASE, SNAKE_MB_PAYLOAD_LEN, payload_len);
    IOWR_32DIRECT(SHARED_FLAGS_BASE, SNAKE_MB_ACK, 0);
    IOWR_32DIRECT(SHARED_FLAGS_BASE, SNAKE_MB_SEQ, seq);

    /* READY must be written last. */
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
static int skip_repeated_command_tail_before_x(
    const char *buf,
    unsigned int *pos,
    unsigned int end,
    unsigned int completed_pair_count
)
{
    unsigned int p;
    unsigned int limit;

    /*
       Rows are fixed-width and commands often split like this:
           SNAKEAPP + LEX1Y1...
           SNAKEWAL + LX5Y1...
           SNAKEPOR + TALAX8Y3...

       If that row appears again while the command is already active, the
       repeated tail can be appended after completed coordinates:
           ...X1Y3LEX1Y1...
           ...X5Y3LX5Y1...
           ...X8Y3TALAX21Y5...

       Old behavior treated the L/LE/TALA text as bad format and reset the
       whole stream. For mixed apple + wall commands, that caused the later
       wall/apple stream to desync.

       After at least one valid XY pair, safely skip alphabetic tail fragments
       until the next X, as long as the X is very close.
    */
    if (completed_pair_count == 0)
    {
        return 0;
    }

    if (*pos >= end)
    {
        return 0;
    }

    if (!isalpha((unsigned char)buf[*pos]))
    {
        return 0;
    }

    if ((end - *pos) >= 5 && strncmp(&buf[*pos], "SNAKE", 5) == 0)
    {
        return 0;
    }

    limit = *pos + 5;
    if (limit > end)
    {
        limit = end;
    }

    for (p = *pos; p < limit; p++)
    {
        if (buf[p] == 'X')
        {
            *pos = p;
            return 1;
        }

        if (!isalpha((unsigned char)buf[p]))
        {
            break;
        }
    }

    return 0;
}

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

    parse_stopped_on_trailing_junk = 0;

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

            if (skip_repeated_command_tail_before_x(buf, &pos, end, count))
            {
                pair_start = pos;
            }
            else
            {
                /*
                   If we already parsed useful coordinates, do not reset the
                   whole decoder just because a trailing duplicated/junk row was
                   appended after them. Return the completed pairs and wait for
                   the next command/chunk.
                */
                if (count > 0)
                {
                    parse_stopped_on_trailing_junk = 1;
                    break;
                }

                return DECODER_ERR_BAD_FORMAT;
            }
        }

        if (pos >= end || buf[pos] != 'X')
        {
            if (count > 0)
            {
                parse_stopped_on_trailing_junk = 1;
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
            /*
               Same protection as above: bad trailing text after at least one
               complete pair should not reset the stream.
            */
            if (count > 0)
            {
                parse_stopped_on_trailing_junk = 1;
                break;
            }

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
   Common OCR repairs after normalization.

   Examples:
       SAKEAPP       -> SNAKEAPP
       NAKEWAL       -> SNAKEWAL
       SNAKEPOR      + TALAX... still becomes SNAKEPORTALAX...
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

        /* Keep only letters and numbers. Removes spaces/newlines. */
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
    int first_nake;
    int first;

    first_snake = find_token_from(0, "SNAKE");
    first_sake = find_token_from(0, "SAKE");
    first_nake = find_token_from(0, "NAKE");

    if (first_snake < 0 && first_sake < 0 && first_nake < 0)
    {
        /* Keep a small suffix in case next chunk completes SNAKE. */
        if (stream_len > 8)
        {
            remove_stream_prefix(stream_len - 8);
        }
        return;
    }

    first = -1;

    if (first_snake >= 0)
    {
        first = first_snake;
    }

    if (first_sake >= 0 && (first < 0 || first_sake < first))
    {
        first = first_sake;
    }

    if (first_nake >= 0 && (first < 0 || first_nake < first))
    {
        first = first_nake;
    }

    if (first > 0)
    {
        remove_stream_prefix((unsigned int)first);
    }

    repair_common_ocr_errors();

    /* If OCR missed the initial S in NAKExxx, repair it after junk trim. */
    if (stream_len >= 4 && strncmp(stream_buf, "NAKE", 4) == 0)
    {
        unsigned int j;

        if (stream_len + 1 < DECODER_STREAM_MAX)
        {
            for (j = stream_len + 1; j > 0; j--)
            {
                stream_buf[j] = stream_buf[j - 1];
            }

            stream_buf[0] = 'S';
            stream_len++;
            stream_buf[stream_len] = '\0';
        }
    }
}

/* ============================================================
   COMMAND SEND HELPERS
   ============================================================ */

static int send_apples_from_segment(unsigned int data_start, unsigned int data_end)
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
       MULTI-APPLE UPDATE:
       Send all completed apple coordinates currently available in the stream.

       This makes apple behave like walls: as more X/Y chunks arrive, the
       decoder can re-send the apple mailbox command with a larger COUNT and
       payload.

       IMPORTANT:
       snake.c / VGA must support COUNT > 1 for SNAKE_CMD_SET_APPLE to display
       multiple apples at once. If snake.c still supports only one apple, it
       will likely use only one coordinate even though the decoder now sends
       all apples.
    */
    if (count == 1)
    {
        return snake_send_mailbox_command(
            SNAKE_CMD_ADD_APPLES,
            count,
            payload_len,
            payload
        );
    }

    return snake_send_mailbox_command(
        SNAKE_CMD_SET_APPLE,
        count,
        payload_len,
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

    if (count == 1)
    {
        return snake_send_mailbox_command(
            SNAKE_CMD_ADD_WALLS,
            count,
            payload_len,
            payload
        );
    }

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

static int send_portals_if_ready(void)
{
    if (portal_a_valid && portal_b_valid)
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

static int send_portal_a_from_segment(unsigned int data_start, unsigned int data_end)
{
    unsigned char payload[4];
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

    /*
       NEW PORTAL FIX:
       Support compact form:
           SNAKEPORTALAX8Y3X21Y5
       The first pair is portal A and the second pair is portal B.
    */
    if (count >= 2 && payload_len >= 4)
    {
        portal_bx = payload[2];
        portal_by = payload[3];
        portal_b_valid = 1;
    }

    return send_portals_if_ready();
}

static int send_portal_b_from_segment(unsigned int data_start, unsigned int data_end)
{
    unsigned char payload[4];
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

    /*
       Optional compact reverse form:
           SNAKEPORTALBX21Y5X8Y3
       The first pair is B and the second pair is A.
    */
    if (count >= 2 && payload_len >= 4)
    {
        portal_ax = payload[2];
        portal_ay = payload[3];
        portal_a_valid = 1;
    }

    return send_portals_if_ready();
}

static int send_portal_auto_from_segment(unsigned int data_start, unsigned int data_end)
{
    unsigned char payload[4];
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
       Support plain form:
           SNAKEPORTALX8Y3X21Y5
       First coordinate becomes A, second coordinate becomes B.
    */
    portal_ax = payload[0];
    portal_ay = payload[1];
    portal_a_valid = 1;

    if (count >= 2 && payload_len >= 4)
    {
        portal_bx = payload[2];
        portal_by = payload[3];
        portal_b_valid = 1;
    }

    return send_portals_if_ready();
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
   DIRECT PORTAL ROW HANDLER

   The scrolling display often sends portal like this:

       snakepor
       talax5y3
       snakepor
       talbx5y6

   Repeated SNAKEPOR rows are normal and must not reset portal A/B state.
   The stream parser can get confused if repeated SNAKEPOR is appended after
   TALAX..., so portal rows are handled directly before the generic stream
   buffer sees them.
   ============================================================ */

static unsigned int normalize_text_to_alnum_upper(
    const char *text,
    unsigned int len,
    char *out,
    unsigned int out_max
)
{
    unsigned int i;
    unsigned int out_len;

    out_len = 0;

    if (out_max == 0)
    {
        return 0;
    }

    for (i = 0; i < len; i++)
    {
        unsigned char c;

        c = (unsigned char)text[i];

        if (c == '\0')
        {
            break;
        }

        if (isalnum(c))
        {
            if (out_len + 1 < out_max)
            {
                out[out_len++] = (char)toupper(c);
            }
        }
    }

    out[out_len] = '\0';
    return out_len;
}

static int local_find_token(const char *buf, unsigned int len, const char *token)
{
    unsigned int i;
    unsigned int token_len;

    token_len = (unsigned int)strlen(token);

    if (token_len == 0 || len < token_len)
    {
        return -1;
    }

    for (i = 0; i + token_len <= len; i++)
    {
        if (strncmp(&buf[i], token, token_len) == 0)
        {
            return (int)i;
        }
    }

    return -1;
}

static int parse_xy_pairs_local(
    const char *buf,
    unsigned int start,
    unsigned int len,
    unsigned char *payload,
    unsigned int max_payload_len,
    unsigned int *out_count,
    unsigned int *out_payload_len
)
{
    unsigned int pos;
    unsigned int count;
    unsigned int payload_len;

    pos = start;
    count = 0;
    payload_len = 0;

    while (pos < len)
    {
        int x;
        int y;

        if (buf[pos] != 'X')
        {
            break;
        }

        pos++;

        if (!parse_number_range(buf, &pos, len, &x))
        {
            break;
        }

        if (pos >= len || buf[pos] != 'Y')
        {
            break;
        }

        pos++;

        if (!parse_number_range(buf, &pos, len, &y))
        {
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

static int direct_store_portal_pairs_from(
    const char *buf,
    unsigned int len,
    unsigned int data_start,
    int first_is_b
)
{
    unsigned char payload[4];
    unsigned int count;
    unsigned int payload_len;
    int result;

    result = parse_xy_pairs_local(
        buf,
        data_start,
        len,
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

    if (first_is_b)
    {
        portal_bx = payload[0];
        portal_by = payload[1];
        portal_b_valid = 1;

        if (count >= 2 && payload_len >= 4)
        {
            portal_ax = payload[2];
            portal_ay = payload[3];
            portal_a_valid = 1;
        }
    }
    else
    {
        portal_ax = payload[0];
        portal_ay = payload[1];
        portal_a_valid = 1;

        if (count >= 2 && payload_len >= 4)
        {
            portal_bx = payload[2];
            portal_by = payload[3];
            portal_b_valid = 1;
        }
    }

    return send_portals_if_ready();
}

static int handle_portal_row_direct(
    const char *text,
    unsigned int len,
    int *out_consumed
)
{
    char n[96];
    unsigned int n_len;
    int p;

    *out_consumed = 0;

    n_len = normalize_text_to_alnum_upper(text, len, n, sizeof(n));

    if (n_len == 0)
    {
        return DECODER_NO_COMMAND;
    }

    /*
       Header row only. It is allowed to repeat and should not clear portal A/B.
       Consume it so the generic stream buffer does not become:
           SNAKEPORTALAX5Y3SNAKEPOR...
    */
    if (local_find_token(n, n_len, "SNAKEPOR") >= 0)
    {
        *out_consumed = 1;

        /*
           If the same row also contains coordinates, handle those below first.
           Header-only rows return no command.
        */
    }

    /*
       A forms accepted:
           TALAX5Y3
           PORTALAX5Y3
           SNAKEPORTALAX5Y3
           AX5Y3
           SNAKEPORTALAX5Y3X21Y5
    */
    p = local_find_token(n, n_len, "PORTALA");
    if (p >= 0)
    {
        int result;

        *out_consumed = 1;
        result = direct_store_portal_pairs_from(n, n_len, (unsigned int)p + 7, 0);

        if (result != DECODER_NO_COMMAND)
        {
            return result;
        }
    }

    p = local_find_token(n, n_len, "TALA");
    if (p >= 0)
    {
        int result;

        *out_consumed = 1;
        result = direct_store_portal_pairs_from(n, n_len, (unsigned int)p + 4, 0);

        if (result != DECODER_NO_COMMAND)
        {
            return result;
        }
    }

    p = local_find_token(n, n_len, "AX");
    if (p >= 0)
    {
        int result;

        *out_consumed = 1;
        result = direct_store_portal_pairs_from(n, n_len, (unsigned int)p + 1, 0);

        if (result != DECODER_NO_COMMAND)
        {
            return result;
        }
    }

    /*
       B forms accepted:
           TALBX5Y6
           PORTALBX5Y6
           SNAKEPORTALBX5Y6
           BX5Y6
           SNAKEPORTALBX5Y6X5Y3
    */
    p = local_find_token(n, n_len, "PORTALB");
    if (p >= 0)
    {
        int result;

        *out_consumed = 1;
        result = direct_store_portal_pairs_from(n, n_len, (unsigned int)p + 7, 1);

        if (result != DECODER_NO_COMMAND)
        {
            return result;
        }
    }

    p = local_find_token(n, n_len, "TALB");
    if (p >= 0)
    {
        int result;

        *out_consumed = 1;
        result = direct_store_portal_pairs_from(n, n_len, (unsigned int)p + 4, 1);

        if (result != DECODER_NO_COMMAND)
        {
            return result;
        }
    }

    p = local_find_token(n, n_len, "BX");
    if (p >= 0)
    {
        int result;

        *out_consumed = 1;
        result = direct_store_portal_pairs_from(n, n_len, (unsigned int)p + 1, 1);

        if (result != DECODER_NO_COMMAND)
        {
            return result;
        }
    }

    if (*out_consumed)
    {
        return DECODER_NO_COMMAND;
    }

    return DECODER_NO_COMMAND;
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
            /*
               Apple integration disabled in this build.
               Keep this parser from blocking later WALL/PORTAL commands.
            */
            result = DECODER_NO_COMMAND;

            if (next_snake > 0)
            {
                remove_after_send = cmd_end;
            }
            else if (stream_len > (unsigned int)strlen("SNAKEAPPLE") + 24)
            {
                remove_after_send = stream_len;
            }
        }
        else if (starts_with_at(stream_buf, 0, "SNAKEADDWALL"))
        {
            result = send_add_walls_from_segment(
                (unsigned int)strlen("SNAKEADDWALL"),
                cmd_end
            );

            if ((next_snake > 0 || parse_stopped_on_trailing_junk) && result >= 0)
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
            if ((next_snake > 0 || parse_stopped_on_trailing_junk) && result >= 0)
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

            /*
               Remove only when both portals are sent, or when another command has
               started. If only A is complete and no next SNAKE, keep the command
               in stream so compact second pair can arrive in the next chunk:
                   snakepor / talax8y3 / x21y5
            */
            if (result == DECODER_OK_SENT)
            {
                remove_after_send = cmd_end;
            }
            else if (result == DECODER_OK_WAITING_PORTAL && next_snake > 0)
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

            if (result == DECODER_OK_SENT)
            {
                remove_after_send = cmd_end;
            }
            else if (result == DECODER_OK_WAITING_PORTAL && next_snake > 0)
            {
                remove_after_send = cmd_end;
            }
        }
        else if (starts_with_at(stream_buf, 0, "SNAKEPORTAL"))
        {
            result = send_portal_auto_from_segment(
                (unsigned int)strlen("SNAKEPORTAL"),
                cmd_end
            );

            if (result == DECODER_OK_SENT)
            {
                remove_after_send = cmd_end;
            }
            else if (result == DECODER_OK_WAITING_PORTAL && next_snake > 0)
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
               Not enough chars yet. Examples:
                   SNAKEWAL
                   SNAKEAPP
                   SNAKEPOR
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

        /* No removal means wait for more chunks. */
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
    int consumed;
    int direct_result;

    if (text == 0 || len == 0)
    {
        return DECODER_NO_COMMAND;
    }

    /*
       Portal rows are handled directly because SNAKEPOR can repeat many times.
       Repeated header rows must not poison the generic stream buffer.
    */
    consumed = 0;
    direct_result = handle_portal_row_direct(text, len, &consumed);

    if (consumed)
    {
        return direct_result;
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
