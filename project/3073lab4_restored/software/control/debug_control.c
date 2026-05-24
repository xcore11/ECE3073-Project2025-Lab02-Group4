#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "system.h"
#include "io.h"
#include "altera_avalon_pio_regs.h"
#include "control.h"

#define DEBUG_MSG_MAX                   64
#define DEBUG_LED_UPDATE_TICKS_PER_SEC  100u   /* leds_update_task updates every ~10 ms */
#define DEBUG_SPEAKER_DEFAULT_FREQ      1000

static uint32_t last_debug_seq = 0;

static char debug_hex_message[DEBUG_MSG_MAX + 1];

static uint32_t led_module_option = 0;
static uint32_t led_module_tick = 0;
static int led_module_blink_visible = 1;

static uint32_t ledr_option = 0;
static uint32_t ledr_tick = 0;
static int ledr_blink_visible = 1;

static uint32_t speaker_option = 0;
static uint32_t speaker_duration_ticks_left = 0;

static uint32_t dbg_read32(uint32_t offset)
{
    return IORD_32DIRECT(DEBUG_CONTROL_BASE, offset);
}

static void dbg_write32(uint32_t offset, uint32_t value)
{
    IOWR_32DIRECT(DEBUG_CONTROL_BASE, offset, value);
}

static uint8_t dbg_read8(uint32_t offset)
{
    return (uint8_t)(IORD_8DIRECT(DEBUG_CONTROL_BASE, offset) & 0xFFu);
}

static void set_all_hex_off(void)
{
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, 0xFF);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX1_BASE, 0xFF);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX2_BASE, 0xFF);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX3_BASE, 0xFF);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX4_BASE, 0xFF);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX5_BASE, 0xFF);
}

static void set_ledr_value(uint32_t value)
{
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_LED_BASE, value & 0x3FFu);
}

void debug_control_all_outputs_off(void)
{
    set_all_hex_off();
    set_ledr_value(0);
    red_light(0);
    yellow_light(0);
    green_light(0);
    play_speaker(0, 0);
}

static void clear_debug_mailbox(void)
{
    int i;

    dbg_write32(FLAG_CONTROL_EVENT_SEQ, 0);
    dbg_write32(FLAG_CONTROL_LAST_EVENT_TYPE, DEBUG_CONTROL_CMD_NONE);
    dbg_write32(FLAG_CONTROL_LAST_EVENT_VALUE, 0);
    dbg_write32(FLAG_CONTROL_SPEAKER_OPTION, 0);
    dbg_write32(FLAG_CONTROL_LED_MODULE, 0);
    dbg_write32(FLAG_CONTROL_LEDR, 0);

    for (i = 0; i < DEBUG_CONTROL_MESSAGE_BYTES; i++) {
        IOWR_8DIRECT(DEBUG_CONTROL_BASE, FLAG_CONTROL_MESSAGE + i, 0);
    }
}

void debug_control_init(void)
{
    last_debug_seq = 0;

    debug_hex_message[0] = '\0';

    led_module_option = 0;
    led_module_tick = 0;
    led_module_blink_visible = 1;

    ledr_option = 0;
    ledr_tick = 0;
    ledr_blink_visible = 1;

    speaker_option = 0;
    speaker_duration_ticks_left = 0;

    clear_debug_mailbox();
    debug_control_all_outputs_off();
}

static void read_debug_message(char *dst, int max_len)
{
    int i;

    if (max_len <= 0) {
        return;
    }

    for (i = 0; i < max_len - 1 && i < DEBUG_MSG_MAX; i++) {
        char c = (char)dbg_read8(FLAG_CONTROL_MESSAGE + i);
        if (c == '\0') {
            break;
        }
        if (c < 32 || c > 126) {
            c = ' ';
        }
        dst[i] = c;
    }

    dst[i] = '\0';
}

void debug_control_copy_message(char *dst, int max_len)
{
    int i;

    if (dst == NULL || max_len <= 0) {
        return;
    }

    for (i = 0; i < max_len - 1 && debug_hex_message[i] != '\0'; i++) {
        dst[i] = debug_hex_message[i];
    }
    dst[i] = '\0';
}

static uint32_t get_seconds_from_option(uint32_t option, uint32_t default_seconds)
{
    uint32_t seconds = (option & DEBUG_OPTION_SECONDS_MASK) >> DEBUG_OPTION_SECONDS_SHIFT;

    if (seconds == 0) {
        seconds = default_seconds;
    }
    if (seconds == 0) {
        seconds = 1;
    }

    return seconds;
}

static void apply_debug_mailbox_update(uint32_t changed_mask)
{
    if ((changed_mask & DEBUG_CONTROL_MASK_HEX_MESSAGE) != 0) {
        read_debug_message(debug_hex_message, sizeof(debug_hex_message));
    }

    if ((changed_mask & DEBUG_CONTROL_MASK_LED_MODULE) != 0) {
        led_module_option = dbg_read32(FLAG_CONTROL_LED_MODULE);
        led_module_tick = 0;
        led_module_blink_visible = 1;
    }

    if ((changed_mask & DEBUG_CONTROL_MASK_LEDR) != 0) {
        ledr_option = dbg_read32(FLAG_CONTROL_LEDR);
        ledr_tick = 0;
        ledr_blink_visible = 1;
    }

    if ((changed_mask & DEBUG_CONTROL_MASK_SPEAKER) != 0) {
        uint32_t seconds;

        speaker_option = dbg_read32(FLAG_CONTROL_SPEAKER_OPTION);

        if ((speaker_option & DEBUG_OPTION_VALID) == 0) {
            speaker_duration_ticks_left = 0;
            play_speaker(0, 0);
        } else if ((speaker_option & DEBUG_SPEAKER_HAS_DURATION) != 0) {
            seconds = get_seconds_from_option(speaker_option, 1);
            speaker_duration_ticks_left = seconds * 1000u;
        } else {
            speaker_duration_ticks_left = 0xFFFFFFFFu;
        }
    }
}

static void set_led_module_bits(uint32_t bits)
{
    red_light((bits & DEBUG_LED_MODULE_RED) ? 1 : 0);
    yellow_light((bits & DEBUG_LED_MODULE_YELLOW) ? 1 : 0);
    green_light((bits & DEBUG_LED_MODULE_GREEN) ? 1 : 0);
}

static void update_led_module_debug(void)
{
    uint32_t bits;
    uint32_t threshold;

    if ((led_module_option & DEBUG_OPTION_VALID) == 0) {
        red_light(0);
        yellow_light(0);
        green_light(0);
        return;
    }

    bits = led_module_option & 0xFFu;

    if ((led_module_option & DEBUG_OPTION_BLINK) == 0) {
        set_led_module_bits(bits);
        return;
    }

    threshold = get_seconds_from_option(led_module_option, 1) * DEBUG_LED_UPDATE_TICKS_PER_SEC;
    if (threshold == 0) {
        threshold = DEBUG_LED_UPDATE_TICKS_PER_SEC;
    }

    led_module_tick++;
    if (led_module_tick >= threshold) {
        led_module_tick = 0;
        led_module_blink_visible = !led_module_blink_visible;
    }

    set_led_module_bits(led_module_blink_visible ? bits : 0);
}

static uint32_t make_ledr_pattern(unsigned int count, int from_left)
{
    uint32_t pattern = 0;
    unsigned int i;

    if (count > 10) {
        count = 10;
    }

    for (i = 0; i < count; i++) {
        if (from_left) {
            pattern |= (1u << (9u - i));
        } else {
            pattern |= (1u << i);
        }
    }

    return pattern & 0x3FFu;
}

static void update_ledr_debug(void)
{
    unsigned int count;
    int from_left;
    uint32_t pattern;
    uint32_t threshold;

    if ((ledr_option & DEBUG_OPTION_VALID) == 0) {
        set_ledr_value(0);
        return;
    }

    count = (unsigned int)(ledr_option & 0xFFu);
    from_left = ((ledr_option & DEBUG_LEDR_FROM_LEFT) != 0);
    pattern = make_ledr_pattern(count, from_left);

    if ((ledr_option & DEBUG_OPTION_BLINK) == 0) {
        set_ledr_value(pattern);
        return;
    }

    threshold = get_seconds_from_option(ledr_option, 1) * DEBUG_LED_UPDATE_TICKS_PER_SEC;
    if (threshold == 0) {
        threshold = DEBUG_LED_UPDATE_TICKS_PER_SEC;
    }

    ledr_tick++;
    if (ledr_tick >= threshold) {
        ledr_tick = 0;
        ledr_blink_visible = !ledr_blink_visible;
    }

    set_ledr_value(ledr_blink_visible ? pattern : 0);
}

void debug_control_update(void)
{
    uint32_t seq = dbg_read32(FLAG_CONTROL_EVENT_SEQ);

    if (seq != last_debug_seq) {
        uint32_t event_type = dbg_read32(FLAG_CONTROL_LAST_EVENT_TYPE);
        uint32_t event_value = dbg_read32(FLAG_CONTROL_LAST_EVENT_VALUE);

        last_debug_seq = seq;

        if (event_type == DEBUG_CONTROL_CMD_BATCH) {
            apply_debug_mailbox_update(event_value);
        }
    }

    /* HEX display is intentionally NOT updated here.
       HEX display now follows the required switch behavior:
       SW1 enables HEX, SW2 scrolls the captured sentence, SW3 shows CPU load. */
    update_led_module_debug();
    update_ledr_debug();
}

void debug_control_service_speaker(int switch_enabled)
{
    int freq;

    if (!switch_enabled) {
        play_speaker(0, 0);
        return;
    }

    if ((speaker_option & DEBUG_OPTION_VALID) != 0) {
        freq = (int)(speaker_option & DEBUG_SPEAKER_FREQ_MASK);
        if (freq <= 0) {
            freq = DEBUG_SPEAKER_DEFAULT_FREQ;
        }

        if ((speaker_option & DEBUG_SPEAKER_HAS_DURATION) != 0) {
            if (speaker_duration_ticks_left == 0) {
                play_speaker(0, 0);
                return;
            }
            speaker_duration_ticks_left--;
        }
    } else {
        freq = DEBUG_SPEAKER_DEFAULT_FREQ;
    }

    play_speaker(freq, 1);
}
