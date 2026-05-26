#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "io.h"
#include "shared_memory.h"
#include "decoderdebug.h"

#define DEBUG_ACCUM_MAX          256
#define DEBUG_MSG_MAX            64

static char debug_accum[DEBUG_ACCUM_MAX];
static char last_hex_msg[DEBUG_MSG_MAX + 1];
static uint32_t last_led_module_option = 0;
static uint32_t last_ledr_option = 0;
static uint32_t last_speaker_option = 0;
static int suppress_next_duration_tail_for_vga = 0;

static void dbg_write32(uint32_t offset, uint32_t value)
{
    IOWR_32DIRECT(DEBUG_CONTROL_BASE, offset, value);
}

static uint32_t dbg_read32(uint32_t offset)
{
    return IORD_32DIRECT(DEBUG_CONTROL_BASE, offset);
}

static void dbg_write_message(const char *msg)
{
    int i;

    for (i = 0; i < DEBUG_CONTROL_MESSAGE_BYTES; i++)
        IOWR_8DIRECT(DEBUG_CONTROL_BASE, FLAG_CONTROL_MESSAGE + i, 0);

    if (msg == 0)
        return;

    for (i = 0; i < DEBUG_MSG_MAX && msg[i] != '\0'; i++)
        IOWR_8DIRECT(DEBUG_CONTROL_BASE, FLAG_CONTROL_MESSAGE + i, (uint8_t)msg[i]);
}

static void publish_debug_update(uint32_t changed_mask,
                                 const char *hex_msg,
                                 uint32_t led_module_option,
                                 uint32_t ledr_option,
                                 uint32_t speaker_option)
{
    uint32_t seq;

    if (changed_mask == 0)
        return;

    if ((changed_mask & DEBUG_CONTROL_MASK_HEX_MESSAGE) != 0)
        dbg_write_message(hex_msg);

    if ((changed_mask & DEBUG_CONTROL_MASK_LED_MODULE) != 0)
        dbg_write32(FLAG_CONTROL_LED_MODULE, led_module_option);

    if ((changed_mask & DEBUG_CONTROL_MASK_LEDR) != 0)
        dbg_write32(FLAG_CONTROL_LEDR, ledr_option);

    if ((changed_mask & DEBUG_CONTROL_MASK_SPEAKER) != 0)
        dbg_write32(FLAG_CONTROL_SPEAKER_OPTION, speaker_option);

    dbg_write32(FLAG_CONTROL_LAST_EVENT_TYPE, DEBUG_CONTROL_CMD_BATCH);
    dbg_write32(FLAG_CONTROL_LAST_EVENT_VALUE, changed_mask);

    seq = dbg_read32(FLAG_CONTROL_EVENT_SEQ) + 1;
    dbg_write32(FLAG_CONTROL_EVENT_SEQ, seq);

    printf("[DEBUG DECODER] published mask=0x%08lX seq=%lu\n",
           (unsigned long)changed_mask,
           (unsigned long)seq);
    fflush(stdout);
}

void decoder_debug_reset(void)
{
    debug_accum[0] = '\0';
    last_hex_msg[0] = '\0';
    last_led_module_option = 0;
    last_ledr_option = 0;
    last_speaker_option = 0;
    suppress_next_duration_tail_for_vga = 0;
}

static void normalize_debug_text(const char *src, unsigned int len, char *dst, unsigned int dst_size);
static void compact_debug_text(const char *src, char *dst, unsigned int dst_size);

static int compact_is_duration_unit_at(const char *p)
{
    if (p == 0)
        return 0;

    /* Match longest first. These are only used in the post-instruction
       suppress window, so we can be generous with OCR variants here. */
    if (strncmp(p, "SECONDS", 7) == 0) return 7;
    if (strncmp(p, "SECOND", 6) == 0)  return 6;
    if (strncmp(p, "SECS", 4) == 0)    return 4;
    if (strncmp(p, "SEC", 3) == 0)     return 3;
    if (strncmp(p, "5EC", 3) == 0)     return 3;
    if (strncmp(p, "S3C", 3) == 0)     return 3;
    if (strncmp(p, "SCC", 3) == 0)     return 3;
    if (strncmp(p, "SE", 2) == 0)      return 2;

    return 0;
}

static int compact_is_only_duration_unit(const char *compact)
{
    unsigned int pos = 0;
    int matched_count = 0;

    if (compact == 0 || compact[0] == '\0')
        return 0;

    /*
       This is intentionally only called after a full debug instruction has
       already been published. At that point a following packet that contains
       only SEC / SECOND / SECSEC is almost always the repeated duration tail
       caused by OCR row duplication. Real SEC rows that are still needed by an
       instruction are not hidden because the suppress window is not armed yet.
    */
    while (compact[pos] != '\0')
    {
        int n = compact_is_duration_unit_at(&compact[pos]);
        if (n <= 0)
            return 0;

        pos += (unsigned int)n;
        matched_count++;
    }

    return matched_count > 0;
}

int decoder_debug_should_hide_vga_text(const char *text, unsigned int len)
{
    char spaced[DEBUG_ACCUM_MAX];
    char compact[DEBUG_ACCUM_MAX];

    if (text == 0 || len == 0)
        return 0;

    if (!suppress_next_duration_tail_for_vga)
        return 0;

    normalize_debug_text(text, len, spaced, sizeof(spaced));
    compact_debug_text(spaced, compact, sizeof(compact));

    if (compact_is_only_duration_unit(compact))
    {
        char accum_compact[DEBUG_ACCUM_MAX];

        compact_debug_text(debug_accum, accum_compact, sizeof(accum_compact));

        /*
           Only suppress a repeated SEC when it is a loose tail immediately
           after a completed instruction.

           If debug_accum already has text, then a new instruction is currently
           being assembled, e.g.

               SET SPEAKER FREQ TO 50 HZ FOR 1
               SEC

           In that case this SEC is required to complete the new instruction,
           so it must still be published to VGA and passed to the decoder.
           This prevents the old global suppress window from eating the SEC of
           the next instruction.
        */
        if (accum_compact[0] != '\0')
        {
            suppress_next_duration_tail_for_vga = 0;
            printf("[DEBUG DECODER] kept SEC because a new instruction is pending: accum=[%s] text=[%s]\n",
                   accum_compact,
                   spaced);
            fflush(stdout);
            return 0;
        }

        suppress_next_duration_tail_for_vga = 0;
        printf("[DEBUG DECODER] suppressed repeated SEC tail for previous instruction: [%s]\n", spaced);
        fflush(stdout);
        return 1;
    }

    /* Only the immediate next non-empty OCR packet is considered a possible
       duplicate tail. If another real word/instruction arrives first, keep it
       and clear the suppression window. */
    if (compact[0] != '\0')
        suppress_next_duration_tail_for_vga = 0;

    return 0;
}

static int is_separator_char(char c)
{
    return !(isalnum((unsigned char)c));
}

static int compact_has_command_start(const char *compact)
{
    if (compact == 0 || compact[0] == '\0')
        return 0;

    return (strstr(compact, "DISPLAY") != 0 ||
            strstr(compact, "HEX") != 0 ||
            strstr(compact, "SETSPEAKER") != 0 ||
            strstr(compact, "SPEAKER") != 0 ||
            strstr(compact, "TURNON") != 0 ||
            strstr(compact, "RED") != 0 ||
            strstr(compact, "GREEN") != 0 ||
            strstr(compact, "YELLOW") != 0);
}

static int compact_is_possible_command_prefix(const char *compact)
{
    const char *starts[] = {
        "DISPLAYHEX", "DISPLAY", "HEX",
        "SETSPEAKER", "SET", "SPEAKER", "FOR",
        "TURNON", "TURN",
        "RED", "GREEN", "YELLOW"
    };
    int i;

    if (compact == 0 || compact[0] == '\0')
        return 0;

    for (i = 0; i < (int)(sizeof(starts) / sizeof(starts[0])); i++)
    {
        size_t n = strlen(compact);
        size_t m = strlen(starts[i]);
        size_t cmp = (n < m) ? n : m;
        if (cmp >= 2 && strncmp(compact, starts[i], cmp) == 0)
            return 1;
    }

    return compact_has_command_start(compact);
}

static void trim_accumulator_to_command_start(void)
{
    const char *needles[] = {
        "DISPLAY HEX", "DISPLAY", "HEX",
        "SET SPEAKER", "SET", "SPEAKER", "FOR",
        "TURN ON", "TURN",
        "RED", "GREEN", "YELLOW"
    };
    const char *best = 0;
    int i;

    for (i = 0; i < (int)(sizeof(needles) / sizeof(needles[0])); i++)
    {
        const char *p = strstr(debug_accum, needles[i]);
        if (p != 0 && (best == 0 || p < best))
            best = p;
    }

    if (best != 0 && best != debug_accum)
        memmove(debug_accum, best, strlen(best) + 1);
}

static void normalize_debug_text(const char *in, unsigned int len, char *out, unsigned int out_size)
{
    unsigned int i;
    unsigned int pos = 0;
    int last_space = 1;

    if (out_size == 0)
        return;

    for (i = 0; i < len && pos < out_size - 1; i++)
    {
        unsigned char uc = (unsigned char)in[i];
        char c = (char)uc;

        if (c == '\0')
            break;

        if (isalnum(uc))
        {
            out[pos++] = (char)toupper(uc);
            last_space = 0;
        }
        else
        {
            if (!last_space && pos < out_size - 1)
            {
                out[pos++] = ' ';
                last_space = 1;
            }
        }
    }

    while (pos > 0 && out[pos - 1] == ' ')
        pos--;

    out[pos] = '\0';
}

static void compact_debug_text(const char *in, char *out, unsigned int out_size)
{
    unsigned int i;
    unsigned int pos = 0;

    if (out_size == 0)
        return;

    for (i = 0; in[i] != '\0' && pos < out_size - 1; i++)
    {
        unsigned char uc = (unsigned char)in[i];
        if (isalnum(uc))
            out[pos++] = (char)toupper(uc);
    }

    out[pos] = '\0';
}


static int compact_duration_unit_len_at(const char *p)
{
    if (p == 0)
        return 0;

    /*
       Match longest first.  This matters because SECOND starts with SEC.
       The OCR/debug path sometimes produces:
           SEC SEC      -> compact SECSEC
           SECOND SEC   -> compact SECONDSEC
           FOR 2 SECSEC -> compact FOR2SECSEC
       The spaced-token filter below catches the first form only when spaces
       survive.  This compact filter catches the no-space forms too.
    */
    if (strncmp(p, "SECONDS", 7) == 0)
        return 7;
    if (strncmp(p, "SECOND", 6) == 0)
        return 6;
    if (strncmp(p, "SEC", 3) == 0)
        return 3;

    return 0;
}

static int filter_duplicate_duration_units_compact(char *compact)
{
    char tmp[DEBUG_ACCUM_MAX];
    unsigned int read_pos = 0;
    unsigned int write_pos = 0;
    int last_token_was_duration = 0;
    int removed = 0;

    if (compact == 0 || compact[0] == '\0')
        return 0;

    while (compact[read_pos] != '\0' && write_pos < sizeof(tmp) - 1)
    {
        int unit_len = compact_duration_unit_len_at(&compact[read_pos]);

        if (unit_len > 0)
        {
            if (last_token_was_duration)
            {
                read_pos += (unsigned int)unit_len;
                removed++;
                continue;
            }

            {
                int i;
                for (i = 0; i < unit_len && write_pos < sizeof(tmp) - 1; i++)
                    tmp[write_pos++] = compact[read_pos++];
            }

            last_token_was_duration = 1;
            continue;
        }

        tmp[write_pos++] = compact[read_pos++];
        last_token_was_duration = 0;
    }

    tmp[write_pos] = '\0';
    strcpy(compact, tmp);

    return removed;
}


static int debug_token_is_duration_unit(const char *tok)
{
    if (tok == 0)
        return 0;

    return (strcmp(tok, "SEC") == 0 ||
            strcmp(tok, "SECOND") == 0 ||
            strcmp(tok, "SECONDS") == 0);
}

static int debug_token_starts_new_instruction(const char *tok)
{
    if (tok == 0)
        return 0;

    return (strcmp(tok, "DISPLAY") == 0 ||
            strcmp(tok, "HEX") == 0 ||
            strcmp(tok, "SET") == 0 ||
            strcmp(tok, "SPEAKER") == 0 ||
            strcmp(tok, "TURN") == 0 ||
            strcmp(tok, "RED") == 0 ||
            strcmp(tok, "GREEN") == 0 ||
            strcmp(tok, "YELLOW") == 0);
}

static int filter_duplicate_duration_units_per_instruction(char *spaced)
{
    char tmp[DEBUG_ACCUM_MAX];
    char tok[32];
    unsigned int read_pos = 0;
    unsigned int write_pos = 0;
    int duration_unit_seen = 0;
    int removed = 0;

    if (spaced == 0 || spaced[0] == '\0')
        return 0;

    tmp[0] = '\0';

    while (spaced[read_pos] != '\0')
    {
        unsigned int tok_pos = 0;
        int is_duration;
        int drop_token = 0;

        while (spaced[read_pos] == ' ')
            read_pos++;

        if (spaced[read_pos] == '\0')
            break;

        while (spaced[read_pos] != '\0' && spaced[read_pos] != ' ' && tok_pos < sizeof(tok) - 1)
            tok[tok_pos++] = spaced[read_pos++];

        tok[tok_pos] = '\0';

        while (spaced[read_pos] != '\0' && spaced[read_pos] != ' ')
            read_pos++;

        if (debug_token_starts_new_instruction(tok))
            duration_unit_seen = 0;

        is_duration = debug_token_is_duration_unit(tok);
        if (is_duration)
        {
            if (duration_unit_seen)
            {
                drop_token = 1;
                removed++;
            }
            else
            {
                duration_unit_seen = 1;
            }
        }

        if (!drop_token)
        {
            unsigned int i;

            if (write_pos > 0 && write_pos < sizeof(tmp) - 1)
                tmp[write_pos++] = ' ';

            for (i = 0; tok[i] != '\0' && write_pos < sizeof(tmp) - 1; i++)
                tmp[write_pos++] = tok[i];
        }
    }

    tmp[write_pos] = '\0';
    strcpy(spaced, tmp);

    return removed;
}

static int compact_has_display_end_marker(const char *compact)
{
    return (compact != 0 && strstr(compact, "ZZZ") != 0);
}

static int compact_is_only_display_end_marker(const char *compact)
{
    return (compact != 0 && strcmp(compact, "ZZZ") == 0);
}

static void append_to_accumulator(const char *text, unsigned int len)
{
    char norm[96];
    char compact_norm[96];
    size_t cur_len;
    size_t add_len;

    normalize_debug_text(text, len, norm, sizeof(norm));
    compact_debug_text(norm, compact_norm, sizeof(compact_norm));

    if (norm[0] == '\0')
        return;

    /*
       ZZZ is ONLY an end delimiter for DISPLAY HEX.  If it appears while no
       DISPLAY HEX command is being accumulated, ignore it completely.  This
       prevents a stale ZZZ row from affecting LED module / LEDR / speaker
       instructions.
    */
    if (compact_is_only_display_end_marker(compact_norm))
    {
        char accum_compact[DEBUG_ACCUM_MAX];
        compact_debug_text(debug_accum, accum_compact, sizeof(accum_compact));
        if (strstr(accum_compact, "DISPLAYHEX") == 0 && strstr(accum_compact, "HEX") == 0)
            return;
    }

    /*
       DEBUG OCR can occasionally enter mid-scroll and send orphan tail pieces
       such as "Z" or "10 HZ" from a previous/partial row.  Do not let those
       poison the accumulator.  Wait until a recognizable command start/prefix
       appears before beginning a new command.
    */
    if (debug_accum[0] == '\0' && !compact_is_possible_command_prefix(compact_norm))
        return;

    cur_len = strlen(debug_accum);
    add_len = strlen(norm);

    if (cur_len + add_len + 2 >= DEBUG_ACCUM_MAX)
    {
        /* If the command has grown too long without completing, resync instead
           of letting stale fragments create one-character/per-fragment updates. */
        debug_accum[0] = '\0';
        if (!compact_is_possible_command_prefix(compact_norm))
            return;
        cur_len = 0;
    }

    if (cur_len > 0 && debug_accum[cur_len - 1] != ' ')
        strncat(debug_accum, " ", DEBUG_ACCUM_MAX - strlen(debug_accum) - 1);

    strncat(debug_accum, norm, DEBUG_ACCUM_MAX - strlen(debug_accum) - 1);
    trim_accumulator_to_command_start();
}

static int parse_uint_after_word(const char *text, const char *word, unsigned int *value)
{
    const char *p = strstr(text, word);

    if (p == 0)
        return 0;

    p += strlen(word);

    while (*p != '\0' && !isdigit((unsigned char)*p))
        p++;

    if (!isdigit((unsigned char)*p))
        return 0;

    *value = 0;
    while (isdigit((unsigned char)*p))
    {
        *value = (*value * 10u) + (unsigned int)(*p - '0');
        p++;
    }

    return 1;
}

static int parse_first_uint(const char *text, unsigned int *value)
{
    const char *p = text;

    while (*p != '\0' && !isdigit((unsigned char)*p))
        p++;

    if (!isdigit((unsigned char)*p))
        return 0;

    *value = 0;
    while (isdigit((unsigned char)*p))
    {
        *value = (*value * 10u) + (unsigned int)(*p - '0');
        p++;
    }

    return 1;
}

static int parse_uint_after_compact_word(const char *compact, const char *word, unsigned int *value)
{
    const char *p = strstr(compact, word);

    if (p == 0)
        return 0;

    p += strlen(word);

    while (*p != '\0' && !isdigit((unsigned char)*p))
        p++;

    if (!isdigit((unsigned char)*p))
        return 0;

    *value = 0;
    while (isdigit((unsigned char)*p))
    {
        *value = (*value * 10u) + (unsigned int)(*p - '0');
        p++;
    }

    return 1;
}

static int compact_has_possible_split_for_after_hz(const char *compact)
{
    const char *p;

    if (compact == 0)
        return 0;

    p = compact;
    while ((p = strstr(p, "HZ")) != 0)
    {
        const char *tail = p + 2;

        /* If a row ends like HZF, the F is probably the first letter of
           FOR from the next OCR row.  Wait instead of publishing a
           continuous speaker command too early. */
        if (tail[0] == 'F')
            return 1;

        p += 2;
    }

    return 0;
}

static const char *last_strstr_local(const char *haystack, const char *needle)
{
    const char *last = 0;
    const char *p = haystack;

    if (haystack == 0 || needle == 0 || needle[0] == '\0')
        return 0;

    while ((p = strstr(p, needle)) != 0)
    {
        last = p;
        p++;
    }

    return last;
}

static int starts_next_instruction_spaced(const char *p)
{
    while (*p == ' ')
        p++;

    if (strncmp(p, "DISPLAY HEX", 11) == 0) return 1;
    if (strncmp(p, "SET SPEAKER", 11) == 0) return 1;
    if (strncmp(p, "TURN ON", 7) == 0) return 1;
    if (strncmp(p, "RED LED", 7) == 0) return 1;
    if (strncmp(p, "GREEN LED", 9) == 0) return 1;
    if (strncmp(p, "YELLOW LED", 10) == 0) return 1;
    if (strncmp(p, "RED AND", 7) == 0) return 1;
    if (strncmp(p, "GREEN AND", 9) == 0) return 1;
    if (strncmp(p, "YELLOW AND", 10) == 0) return 1;

    return 0;
}

static int compact_next_instruction_index(const char *p)
{
    const char *best = 0;
    const char *q;
    const char *needles[] = {
        "REDLEDMODULE", "GREENLEDMODULE", "YELLOWLEDMODULE",
        "REDAND", "GREENAND", "YELLOWAND",
        "TURNON", "SETSPEAKER", "DISPLAYHEX"
    };
    int i;

    for (i = 0; i < (int)(sizeof(needles) / sizeof(needles[0])); i++)
    {
        q = strstr(p, needles[i]);
        if (q != 0 && (best == 0 || q < best))
            best = q;
    }

    if (best == 0)
        return -1;

    return (int)(best - p);
}

static int extract_hex_message(const char *spaced, const char *compact, char *msg_out)
{
    const char *p;
    const char *end;
    unsigned int i = 0;

    msg_out[0] = '\0';

    /*
       DISPLAY HEX is the only DEBUG instruction with arbitrary-length payload.
       Therefore it must not be published until the Python scroll sends the
       explicit ZZZ delimiter row.  Without this, a partial row like "HEX 1234"
       can publish early and the following rows are ignored as unrelated text.
    */
    p = last_strstr_local(spaced, "DISPLAY HEX");
    if (p != 0)
    {
        p += strlen("DISPLAY HEX");
        while (*p == ' ')
            p++;
    }
    else
    {
        p = last_strstr_local(spaced, "HEX");
        if (p != 0)
        {
            p += strlen("HEX");
            while (*p == ' ')
                p++;
        }
    }

    if (p != 0)
    {
        end = strstr(p, "ZZZ");
        if (end == 0)
            return 0; /* DISPLAY HEX is still incomplete. */

        while (p < end && *p != '\0' && i < DEBUG_MSG_MAX)
        {
            char c = *p++;
            if (isalnum((unsigned char)c) || c == ' ')
                msg_out[i++] = c;
        }

        while (i > 0 && msg_out[i - 1] == ' ')
            i--;
        msg_out[i] = '\0';
        return (i > 0);
    }

    p = last_strstr_local(compact, "DISPLAYHEX");
    if (p != 0)
        p += strlen("DISPLAYHEX");
    else
    {
        p = last_strstr_local(compact, "HEX");
        if (p != 0)
            p += strlen("HEX");
    }

    if (p == 0 || *p == '\0')
        return 0;

    end = strstr(p, "ZZZ");
    if (end == 0)
        return 0; /* DISPLAY HEX is still incomplete. */

    while (p < end && *p != '\0' && i < DEBUG_MSG_MAX)
    {
        char c = *p++;
        if (isalnum((unsigned char)c))
            msg_out[i++] = c;
    }

    msg_out[i] = '\0';
    return (i > 0);
}

static int color_bits_from_text(const char *compact)
{
    int bits = 0;

    if (strstr(compact, "RED") != 0)
        bits |= DEBUG_LED_MODULE_RED;
    if (strstr(compact, "YELLOW") != 0 || strstr(compact, "YEL") != 0)
        bits |= DEBUG_LED_MODULE_YELLOW;
    if (strstr(compact, "GREEN") != 0 || strstr(compact, "GRN") != 0)
        bits |= DEBUG_LED_MODULE_GREEN;

    return bits;
}

static int detect_led_module(const char *spaced, const char *compact, uint32_t *option_out)
{
    unsigned int seconds = 0;
    int colors;
    int blink;

    if (strstr(compact, "LEDMODULE") == 0)
        return 0;

    /* Do not publish just because "LED MODULE" was seen.  Wait until the
       instruction is complete: either STATIC, or BLINK...SECOND/SEC. */
    if (strstr(compact, "BLINK") == 0 && strstr(compact, "STATIC") == 0)
        return 0;

    colors = color_bits_from_text(compact);
    if (colors == 0)
        colors = DEBUG_LED_MODULE_RED;

    blink = (strstr(compact, "BLINK") != 0);

    if (blink)
    {
        if (strstr(compact, "SECOND") == 0 && strstr(compact, "SEC") == 0)
            return 0;

        if (!parse_uint_after_word(spaced, "EVERY", &seconds))
            parse_first_uint(spaced, &seconds);

        if (seconds == 0)
            return 0;
    }

    *option_out = DEBUG_OPTION_VALID |
                  (uint32_t)(colors & 0xFF) |
                  (blink ? DEBUG_OPTION_BLINK : 0u) |
                  ((uint32_t)(seconds & 0xFFu) << DEBUG_OPTION_SECONDS_SHIFT);
    return 1;
}


static int compact_has_partial_blink_word(const char *compact)
{
    if (compact == 0)
        return 0;

    /* OCR packets may split BLINKING as BLI / BLIN / BLINK / BLINKIN.
       If any of these tails are already visible, the instruction is intended
       to be a blinking command, so do not publish a static LED/LEDR command
       early. */
    return (strstr(compact, "BLI") != 0 ||
            strstr(compact, "BLIN") != 0 ||
            strstr(compact, "BLINK") != 0);
}

static int compact_has_second_word(const char *compact)
{
    if (compact == 0)
        return 0;
    return (strstr(compact, "SECOND") != 0 || strstr(compact, "SEC") != 0);
}

static int detect_ledr(const char *spaced, const char *compact, uint32_t *option_out)
{
    unsigned int count = 0;
    unsigned int seconds = 0;
    int from_left;
    int blink;

    if (strstr(compact, "TURNON") == 0 || strstr(compact, "LED") == 0)
        return 0;

    {
        const char *turn = strstr(spaced, "TURN ON");
        if (turn == 0 || !parse_first_uint(turn, &count))
            return 0;
    }

    if (count == 0)
        return 0;
    if (count > 10)
        count = 10;

    if (strstr(compact, "FROMTHELEFT") != 0 || strstr(compact, "FROMLEFT") != 0)
        from_left = 1;
    else if (strstr(compact, "FROMTHERIGHT") != 0 || strstr(compact, "FROMRIGHT") != 0)
        from_left = 0;
    else
        return 0;

    /*
       Do not publish the LEDR command too early.  The OCR can produce:
           TURN ON / 5 LED FR / OM THE L / EFT BLIN / KING EVE / RY 1 SEC / OND
       At "EFT BLIN" we already know this is a blinking command, but the old
       decoder published a static LEDR update before the duration arrived.
    */
    if (compact_has_partial_blink_word(compact) && !compact_has_second_word(compact))
        return 0;

    blink = (strstr(compact, "BLINK") != 0 || compact_has_partial_blink_word(compact));
    if (blink)
    {
        if (!compact_has_second_word(compact))
            return 0;

        if (!parse_uint_after_word(spaced, "EVERY", &seconds))
            seconds = 1;
    }

    *option_out = DEBUG_OPTION_VALID |
                  (count & 0xFFu) |
                  (from_left ? DEBUG_LEDR_FROM_LEFT : 0u) |
                  (blink ? DEBUG_OPTION_BLINK : 0u) |
                  ((seconds & 0xFFu) << DEBUG_OPTION_SECONDS_SHIFT);
    return 1;
}

static int detect_speaker(const char *spaced, const char *compact, uint32_t *option_out)
{
    unsigned int freq = 0;
    unsigned int seconds = 0;
    int has_duration = 0;

    if (strstr(compact, "SPEAKER") == 0)
        return 0;

    /* Python now emits OCR-friendlier "FREQ" instead of the 9-letter
       "FREQUENCY" when possible.  Accept both forms. */
    if (strstr(compact, "FREQUENCY") == 0 && strstr(compact, "FREQ") == 0)
        return 0;

    if (strstr(compact, "HZ") == 0)
        return 0;

    {
        const char *speaker = strstr(spaced, "SPEAKER");
        if (speaker == 0 || !parse_uint_after_word(speaker, "TO", &freq))
            parse_first_uint(speaker ? speaker : spaced, &freq);
    }

    if (freq == 0)
        return 0;
    if (freq > 65535u)
        freq = 65535u;

    /*
       Support both DEBUG speaker styles:

       1) SET SPEAKER FREQUENCY TO 1000 HZ
          -> continuous until DEBUG exits or SW4 turns off.

       2) SET SPEAKER FREQUENCY TO 1000 HZ FOR 5 SECOND
          -> auto-off after 5 seconds.

       The command may arrive as multiple OCR packets.  If the "FOR ... SECOND"
       tail arrives in the same accumulator, publish the duration immediately.
       If it arrives later, detect_speaker_duration_tail() below updates the
       previous speaker command with the duration.
    */
    if (strstr(compact, "FOR") != 0)
    {
        if (strstr(compact, "SECOND") == 0 && strstr(compact, "SEC") == 0)
            return 0;

        if (!parse_uint_after_word(spaced, "FOR", &seconds) &&
            !parse_uint_after_compact_word(compact, "FOR", &seconds))
            parse_first_uint(spaced, &seconds);

        if (seconds == 0)
            return 0;

        if (seconds > 255u)
            seconds = 255u;

        has_duration = 1;
    }
    else if (compact_has_possible_split_for_after_hz(compact))
    {
        return 0;
    }

    *option_out = DEBUG_OPTION_VALID |
                  (freq & DEBUG_SPEAKER_FREQ_MASK) |
                  (has_duration ? DEBUG_SPEAKER_HAS_DURATION : 0u) |
                  ((uint32_t)(seconds & 0xFFu) << DEBUG_OPTION_SECONDS_SHIFT);
    return 1;
}

static int display_hex_is_waiting_for_zzz(const char *spaced, const char *compact)
{
    const char *p_spaced;
    const char *p_compact;

    p_spaced = last_strstr_local(spaced, "DISPLAY HEX");
    if (p_spaced == 0)
        p_spaced = last_strstr_local(spaced, "HEX");

    p_compact = last_strstr_local(compact, "DISPLAYHEX");
    if (p_compact == 0)
        p_compact = last_strstr_local(compact, "HEX");

    if (p_spaced == 0 && p_compact == 0)
        return 0;

    return !compact_has_display_end_marker(compact);
}

static int detect_speaker_duration_tail(const char *spaced, const char *compact, uint32_t *option_out)
{
    unsigned int seconds = 0;
    uint32_t freq;

    /*
       If the speaker command was already published at "... HZ", the later
       OCR fragments may only contain "FOR 5 SECOND" without the words
       SPEAKER/FREQUENCY.  Use the previous speaker frequency and republish it
       as a duration-limited command.
    */
    if ((last_speaker_option & DEBUG_OPTION_VALID) == 0)
        return 0;

    if (strstr(compact, "FOR") == 0 && strstr(compact, "OR") == 0)
        return 0;

    if (strstr(compact, "SECOND") == 0 && strstr(compact, "SEC") == 0)
        return 0;

    if (!parse_uint_after_word(spaced, "FOR", &seconds) &&
        !parse_uint_after_compact_word(compact, "FOR", &seconds) &&
        !parse_uint_after_compact_word(compact, "OR", &seconds))
        parse_first_uint(spaced, &seconds);

    if (seconds == 0)
        return 0;
    if (seconds > 255u)
        seconds = 255u;

    freq = last_speaker_option & DEBUG_SPEAKER_FREQ_MASK;
    if (freq == 0)
        return 0;

    *option_out = DEBUG_OPTION_VALID |
                  DEBUG_SPEAKER_HAS_DURATION |
                  freq |
                  ((uint32_t)(seconds & 0xFFu) << DEBUG_OPTION_SECONDS_SHIFT);
    return 1;
}

int decoder_debug_decode_text(const char *text, unsigned int len)
{
    char spaced[DEBUG_ACCUM_MAX];
    char compact[DEBUG_ACCUM_MAX];
    char hex_msg[DEBUG_MSG_MAX + 1];
    uint32_t changed_mask = 0;
    uint32_t led_module_option = 0;
    uint32_t ledr_option = 0;
    uint32_t speaker_option = 0;

    if (text == 0 || len == 0)
        return DEBUG_DECODER_NO_COMMAND;

    append_to_accumulator(text, len);

    normalize_debug_text(debug_accum, (unsigned int)strlen(debug_accum), spaced, sizeof(spaced));
    compact_debug_text(spaced, compact, sizeof(compact));

    /* A standalone ZZZ is only the DISPLAY HEX delimiter.  If it arrives after
       the display command has already been handled or while no DISPLAY HEX is
       active, use it only to resync the accumulator. */
    if (compact_is_only_display_end_marker(compact))
    {
        debug_accum[0] = '\0';
        return DEBUG_DECODER_NO_COMMAND;
    }

    if (extract_hex_message(spaced, compact, hex_msg))
    {
        if (strcmp(hex_msg, last_hex_msg) != 0)
        {
            strncpy(last_hex_msg, hex_msg, DEBUG_MSG_MAX);
            last_hex_msg[DEBUG_MSG_MAX] = '\0';
            changed_mask |= DEBUG_CONTROL_MASK_HEX_MESSAGE;
        }
    }
    else if (display_hex_is_waiting_for_zzz(spaced, compact))
    {
        /* Keep collecting DISPLAY HEX payload rows until the explicit ZZZ
           delimiter arrives.  Do not let arbitrary display text accidentally
           trigger LED/speaker parsing. */
        return DEBUG_DECODER_NO_COMMAND;
    }

    if (detect_led_module(spaced, compact, &led_module_option))
    {
        if (led_module_option != last_led_module_option)
        {
            last_led_module_option = led_module_option;
            changed_mask |= DEBUG_CONTROL_MASK_LED_MODULE;
        }
    }

    if (detect_ledr(spaced, compact, &ledr_option))
    {
        if (ledr_option != last_ledr_option)
        {
            last_ledr_option = ledr_option;
            changed_mask |= DEBUG_CONTROL_MASK_LEDR;
        }
    }

    if (detect_speaker(spaced, compact, &speaker_option) ||
        detect_speaker_duration_tail(spaced, compact, &speaker_option))
    {
        if (speaker_option != last_speaker_option)
        {
            last_speaker_option = speaker_option;
            changed_mask |= DEBUG_CONTROL_MASK_SPEAKER;
        }
    }

    if (changed_mask != 0)
    {
        publish_debug_update(changed_mask,
                             last_hex_msg,
                             last_led_module_option,
                             last_ledr_option,
                             last_speaker_option);

        if ((changed_mask & (DEBUG_CONTROL_MASK_LED_MODULE | DEBUG_CONTROL_MASK_LEDR | DEBUG_CONTROL_MASK_SPEAKER)) != 0 &&
            compact_has_second_word(compact))
        {
            suppress_next_duration_tail_for_vga = 1;
        }

        /* The instruction is complete and has been published.  Clear the
           accumulator so trailing OCR fragments such as the final "Z" in
           "HZ" or stale repeated rows cannot republish/overreach into the
           next instruction or cause one-character updates. */
        debug_accum[0] = '\0';
        return DEBUG_DECODER_PUBLISHED;
    }

    return DEBUG_DECODER_NO_COMMAND;
}
