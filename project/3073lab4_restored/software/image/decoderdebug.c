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

    if (strstr(compact, "SPEAKER") == 0 || strstr(compact, "FREQUENCY") == 0)
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

        if (!parse_uint_after_word(spaced, "FOR", &seconds))
            parse_first_uint(spaced, &seconds);

        if (seconds == 0)
            return 0;

        if (seconds > 255u)
            seconds = 255u;

        has_duration = 1;
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

    if (strstr(compact, "FOR") == 0)
        return 0;

    if (strstr(compact, "SECOND") == 0 && strstr(compact, "SEC") == 0)
        return 0;

    if (!parse_uint_after_word(spaced, "FOR", &seconds))
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

        /* The instruction is complete and has been published.  Clear the
           accumulator so trailing OCR fragments such as the final "Z" in
           "HZ" or stale repeated rows cannot republish/overreach into the
           next instruction or cause one-character updates. */
        debug_accum[0] = '\0';
        return DEBUG_DECODER_PUBLISHED;
    }

    return DEBUG_DECODER_NO_COMMAND;
}
