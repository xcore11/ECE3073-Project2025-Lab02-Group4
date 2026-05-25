#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "system.h"
#include "io.h"
#include "altera_avalon_pio_regs.h"
#include "control.h"

#define DEBUG_MSG_MAX                   64
#define DEBUG_LED_BLINK_DEFAULT_MS      1000u
#define DEBUG_SPEAKER_DEFAULT_FREQ      1000
#define DEBUG_SPEAKER_MIN_HALF_TICKS   1u

#ifndef OS_TICKS_PER_SEC
#define OS_TICKS_PER_SEC 100u
#endif

static uint32_t ms_to_os_ticks_local(uint32_t ms)
{
    uint32_t ticks = (uint32_t)(((uint64_t)ms * (uint64_t)OS_TICKS_PER_SEC + 999u) / 1000u);
    return (ticks == 0u) ? 1u : ticks;
}

static uint32_t seconds_to_os_ticks_local(uint32_t seconds)
{
    if (seconds == 0u)
        seconds = 1u;
    return ms_to_os_ticks_local(seconds * 1000u);
}

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
static uint32_t speaker_last_service_tick = 0;
static uint32_t speaker_last_toggle_tick = 0;
static int speaker_output_level = 0;

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
    led_module_tick = (uint32_t)OSTimeGet();
    led_module_blink_visible = 1;

    ledr_option = 0;
    ledr_tick = (uint32_t)OSTimeGet();
    ledr_blink_visible = 1;

    speaker_option = 0;
    speaker_duration_ticks_left = 0;
    speaker_last_service_tick = (uint32_t)OSTimeGet();
    speaker_last_toggle_tick = (uint32_t)OSTimeGet();
    speaker_output_level = 0;

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
        led_module_tick = (uint32_t)OSTimeGet();
        led_module_blink_visible = 1;
    }

    if ((changed_mask & DEBUG_CONTROL_MASK_LEDR) != 0) {
        ledr_option = dbg_read32(FLAG_CONTROL_LEDR);
        ledr_tick = (uint32_t)OSTimeGet();
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
            speaker_duration_ticks_left = seconds_to_os_ticks_local(seconds);
            speaker_last_service_tick = (uint32_t)OSTimeGet();
            speaker_last_toggle_tick = (uint32_t)OSTimeGet();
            speaker_output_level = 0;
        } else {
            speaker_duration_ticks_left = 0xFFFFFFFFu;
            speaker_last_service_tick = (uint32_t)OSTimeGet();
            speaker_last_toggle_tick = (uint32_t)OSTimeGet();
            speaker_output_level = 0;
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

    threshold = seconds_to_os_ticks_local(get_seconds_from_option(led_module_option, 1));

    {
        uint32_t now_ticks = (uint32_t)OSTimeGet();
        if ((uint32_t)(now_ticks - led_module_tick) >= threshold) {
            led_module_tick = now_ticks;
            led_module_blink_visible = !led_module_blink_visible;
        }
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

    threshold = seconds_to_os_ticks_local(get_seconds_from_option(ledr_option, 1));

    {
        uint32_t now_ticks = (uint32_t)OSTimeGet();
        if ((uint32_t)(now_ticks - ledr_tick) >= threshold) {
            ledr_tick = now_ticks;
            ledr_blink_visible = !ledr_blink_visible;
        }
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

static void debug_speaker_force_off(void)
{
    speaker_output_level = 0;
    speaker_last_toggle_tick = (uint32_t)OSTimeGet();
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_SPEAKER_BASE, 0);
    play_speaker(0, 0);
}

static uint32_t debug_speaker_half_period_ticks(int freq)
{
    uint32_t half_period_us;
    uint32_t ticks;

    if (freq <= 0) {
        return 0u;
    }

    /*
       RTOS-only speaker mode.

       The speaker is toggled from the RTOS speaker task, so the real maximum
       frequency is limited by the OS tick rate.  If freq is higher than what
       the tick can represent, it is clamped to one toggle per RTOS tick rather
       than becoming intermittent.  This is why very high commands like
       10000 Hz will sound like the highest stable RTOS-tick tone, not a true
       10000 Hz hardware-PWM tone.
    */
    half_period_us = (uint32_t)((500000u + (uint32_t)freq - 1u) / (uint32_t)freq);
    if (half_period_us == 0u) {
        half_period_us = 1u;
    }

    ticks = (uint32_t)(((uint64_t)half_period_us * (uint64_t)OS_TICKS_PER_SEC + 999999u) / 1000000u);
    if (ticks < DEBUG_SPEAKER_MIN_HALF_TICKS) {
        ticks = DEBUG_SPEAKER_MIN_HALF_TICKS;
    }

    return ticks;
}

static void run_debug_speaker_rtos_step(int freq)
{
    uint32_t now_ticks;
    uint32_t half_ticks;

    if (freq <= 0) {
        debug_speaker_force_off();
        return;
    }

    half_ticks = debug_speaker_half_period_ticks(freq);
    if (half_ticks == 0u) {
        debug_speaker_force_off();
        return;
    }

    now_ticks = (uint32_t)OSTimeGet();

    if ((uint32_t)(now_ticks - speaker_last_toggle_tick) >= half_ticks) {
        speaker_last_toggle_tick = now_ticks;
        speaker_output_level = !speaker_output_level;
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_SPEAKER_BASE, speaker_output_level ? 1 : 0);
    }
}

void debug_control_service_speaker(int switch_enabled)
{
    int freq;

    /*
       SW4 is the master speaker enable. No decoded DEBUG speaker command means
       forced OFF; there is no default/test tone.
    */
    if (!switch_enabled || ((speaker_option & DEBUG_OPTION_VALID) == 0)) {
        debug_speaker_force_off();
        return;
    }

    freq = (int)(speaker_option & DEBUG_SPEAKER_FREQ_MASK);
    if (freq <= 0) {
        debug_speaker_force_off();
        return;
    }

    /*
       Duration support for DEBUG speaker:
       - No FOR clause: continuous until SW4 OFF or DEBUG exits.
       - FOR <n> SECOND: auto-off after n seconds.

       Count down using RTOS ticks so the speaker remains RTOS-serviced rather
       than busy-waiting.
    */
    if ((speaker_option & DEBUG_SPEAKER_HAS_DURATION) != 0) {
        uint32_t now_ticks = (uint32_t)OSTimeGet();
        uint32_t elapsed_ticks = now_ticks - speaker_last_service_tick;
        speaker_last_service_tick = now_ticks;

        if (speaker_duration_ticks_left == 0u || elapsed_ticks >= speaker_duration_ticks_left) {
            speaker_duration_ticks_left = 0u;
            speaker_option = 0u;
            debug_speaker_force_off();
            return;
        }

        speaker_duration_ticks_left -= elapsed_ticks;
    }

    run_debug_speaker_rtos_step(freq);
}
