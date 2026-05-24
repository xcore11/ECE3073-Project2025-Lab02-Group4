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

static void append_to_accumulator(const char *text, unsigned int len)
{
    char norm[96];
    size_t cur_len;
    size_t add_len;

    normalize_debug_text(text, len, norm, sizeof(norm));

    if (norm[0] == '\0')
        return;

    cur_len = strlen(debug_accum);
    add_len = strlen(norm);

    if (cur_len + add_len + 2 >= DEBUG_ACCUM_MAX)
    {
        /* Keep the newest context. Old debug rows have already been decoded. */
        size_t keep = DEBUG_ACCUM_MAX / 2;
        size_t old_len = strlen(debug_accum);
        if (old_len > keep)
        {
            memmove(debug_accum, debug_accum + old_len - keep, keep + 1);
            cur_len = strlen(debug_accum);
        }
    }

    if (cur_len > 0 && debug_accum[cur_len - 1] != ' ')
        strncat(debug_accum, " ", DEBUG_ACCUM_MAX - strlen(debug_accum) - 1);

    strncat(debug_accum, norm, DEBUG_ACCUM_MAX - strlen(debug_accum) - 1);
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
    unsigned int i = 0;

    msg_out[0] = '\0';

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

    if (p != 0 && *p != '\0')
    {
        while (*p != '\0' && i < DEBUG_MSG_MAX)
        {
            char c;

            if (i > 0 && starts_next_instruction_spaced(p))
                break;

            c = *p++;
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

    {
        int stop = compact_next_instruction_index(p);
        int limit = (stop >= 0) ? stop : (int)strlen(p);

        while (*p != '\0' && i < DEBUG_MSG_MAX && limit > 0)
        {
            char c = *p++;
            if (isalnum((unsigned char)c))
                msg_out[i++] = c;
            limit--;
        }
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

    colors = color_bits_from_text(compact);
    if (colors == 0)
        colors = DEBUG_LED_MODULE_RED;

    blink = (strstr(compact, "BLINK") != 0);

    if (blink)
    {
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

    blink = (strstr(compact, "BLINK") != 0);
    if (blink)
    {
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

    if (parse_uint_after_word(spaced, "FOR", &seconds))
        has_duration = 1;

    *option_out = DEBUG_OPTION_VALID |
                  (freq & DEBUG_SPEAKER_FREQ_MASK) |
                  (has_duration ? DEBUG_SPEAKER_HAS_DURATION : 0u) |
                  ((seconds & 0xFFu) << DEBUG_OPTION_SECONDS_SHIFT);
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

    if (extract_hex_message(spaced, compact, hex_msg))
    {
        if (strcmp(hex_msg, last_hex_msg) != 0)
        {
            strncpy(last_hex_msg, hex_msg, DEBUG_MSG_MAX);
            last_hex_msg[DEBUG_MSG_MAX] = '\0';
            changed_mask |= DEBUG_CONTROL_MASK_HEX_MESSAGE;
        }
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

    if (detect_speaker(spaced, compact, &speaker_option))
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
        return DEBUG_DECODER_PUBLISHED;
    }

    return DEBUG_DECODER_NO_COMMAND;
}
