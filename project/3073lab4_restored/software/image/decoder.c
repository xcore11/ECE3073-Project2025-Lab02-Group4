#include "io.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "shared_memory.h"
#include "decoder.h"

/* =========================
   Local decoder settings
   ========================= */

#define DECODER_TEXT_MAX        TEXT_BUFFER_SIZE
#define LOCAL_MSG_MAX           HEX_MSG_MAX_LEN

static uint32_t command_sequence = 0;

/* =========================
   SDRAM helpers
   ========================= */

static uint32_t read_status(uint32_t offset)
{
    return IORD_32DIRECT(STATUS_BASE, offset);
}

static void write_status(uint32_t offset, uint32_t value)
{
    IOWR_32DIRECT(STATUS_BASE, offset, value);
}

static void write_instr(uint32_t offset, uint32_t value)
{
    IOWR_32DIRECT(INSTRUCTION_BASE, offset, value);
}

static uint32_t read_instr(uint32_t offset)
{
    return IORD_32DIRECT(INSTRUCTION_BASE, offset);
}

static void clear_hex_message_buffer(void)
{
    volatile char *hex_msg = (volatile char *)(INSTRUCTION_BASE + HEX_MSG_BASE);
    int i;

    for (i = 0; i < HEX_MSG_MAX_LEN; i++) {
        hex_msg[i] = '\0';
    }
}

static void write_hex_message(const char *msg)
{
    volatile char *hex_msg = (volatile char *)(INSTRUCTION_BASE + HEX_MSG_BASE);
    int i;

    clear_hex_message_buffer();

    for (i = 0; i < HEX_MSG_MAX_LEN - 1 && msg[i] != '\0'; i++) {
        hex_msg[i] = msg[i];
    }

    hex_msg[i] = '\0';
    write_instr(HEX_MSG_LEN, (uint32_t)i);
}

static void clear_instruction_registers(void)
{
    int i;

    for (i = 0; i < INSTRUCTION_SIZE; i += 4) {
        IOWR_32DIRECT(INSTRUCTION_BASE, i, 0);
    }

    clear_hex_message_buffer();
}

/* =========================
   Text helpers
   ========================= */

static int is_upper_char(char c)
{
    return (c >= 'A' && c <= 'Z');
}

static int is_digit_char(char c)
{
    return (c >= '0' && c <= '9');
}

static int is_alnum_or_space(char c)
{
    if (c >= 'a' && c <= 'z') return 1;
    if (c >= 'A' && c <= 'Z') return 1;
    if (c >= '0' && c <= '9') return 1;
    if (c == ' ') return 1;
    return 0;
}

static void normalize_text(char *text)
{
    int i;
    int j = 0;
    char temp[DECODER_TEXT_MAX];

    for (i = 0; i < DECODER_TEXT_MAX; i++) {
        temp[i] = '\0';
    }

    /*
       Convert to lowercase.
       Keep letters, numbers, and spaces.
       Convert underscore/dash to space.
       Remove weird symbols.
    */
    for (i = 0; text[i] != '\0' && j < DECODER_TEXT_MAX - 1; i++) {
        char c = text[i];

        if (is_upper_char(c)) {
            c = c + ('a' - 'A');
        }

        if (c == '_' || c == '-') {
            temp[j++] = ' ';
        } else if (is_alnum_or_space(c)) {
            temp[j++] = c;
        }
    }

    temp[j] = '\0';

    for (i = 0; i < DECODER_TEXT_MAX; i++) {
        text[i] = temp[i];
    }
}

static int contains_word(const char *text, const char *key)
{
    return strstr(text, key) != NULL;
}

static int find_first_number(const char *text)
{
    int i;

    for (i = 0; text[i] != '\0'; i++) {
        if (is_digit_char(text[i])) {
            return atoi(&text[i]);
        }
    }

    return -1;
}

static int find_largest_number(const char *text)
{
    int i;
    int best = -1;

    for (i = 0; text[i] != '\0'; i++) {
        if (is_digit_char(text[i])) {
            int value = atoi(&text[i]);

            if (value > best) {
                best = value;
            }

            while (is_digit_char(text[i])) {
                i++;
            }

            if (text[i] == '\0') {
                break;
            }
        }
    }

    return best;
}

static int find_number_after_key(const char *text, const char *key)
{
    char *p = strstr((char *)text, key);

    if (p == NULL) {
        return -1;
    }

    p += strlen(key);

    while (*p != '\0' && !is_digit_char(*p)) {
        p++;
    }

    if (*p == '\0') {
        return -1;
    }

    return atoi(p);
}

static int find_number_before_key(const char *text, const char *key)
{
    char *p = strstr((char *)text, key);
    int end;
    int start;
    char num_buf[16];
    int n = 0;

    if (p == NULL) {
        return -1;
    }

    end = (int)(p - text) - 1;

    while (end >= 0 && !is_digit_char(text[end])) {
        end--;
    }

    if (end < 0) {
        return -1;
    }

    start = end;

    while (start >= 0 && is_digit_char(text[start])) {
        start--;
    }

    start++;

    while (start <= end && n < 15) {
        num_buf[n++] = text[start++];
    }

    num_buf[n] = '\0';

    return atoi(num_buf);
}

static int find_period_ms(const char *text)
{
    int value;

    /*
       Prefer number before second/sec.
       Examples:
       "every 5 second"
       "5 second"
       "5sec"
    */
    value = find_number_before_key(text, "seconds");
    if (value > 0) return value * 1000;

    value = find_number_before_key(text, "second");
    if (value > 0) return value * 1000;

    value = find_number_before_key(text, "secs");
    if (value > 0) return value * 1000;

    value = find_number_before_key(text, "sec");
    if (value > 0) return value * 1000;

    /*
       Backup:
       number after "every"
    */
    value = find_number_after_key(text, "every");
    if (value > 0) return value * 1000;

    /*
       Backup:
       number after "blink"
    */
    value = find_number_after_key(text, "blink");
    if (value > 0) return value * 1000;

    return -1;
}

static int has_blink_mode(const char *text)
{
    if (contains_word(text, "blinking")) return 1;
    if (contains_word(text, "blink")) return 1;
    if (contains_word(text, "every")) return 1;

    return 0;
}

static int has_static_mode(const char *text)
{
    if (contains_word(text, "static")) return 1;

    return 0;
}

/* =========================
   HEX message extraction
   ========================= */

static int find_marker_index(const char *text, const char *key)
{
    char *p = strstr((char *)text, key);

    if (p == NULL) {
        return -1;
    }

    return (int)(p - text);
}

static int min_positive_index(int current, int candidate)
{
    if (candidate < 0) return current;
    if (current < 0) return candidate;
    if (candidate < current) return candidate;
    return current;
}

static void extract_hex_message(const char *text, char *out_msg)
{
    int start = -1;
    int end = -1;
    int i;
    int j = 0;

    for (i = 0; i < LOCAL_MSG_MAX; i++) {
        out_msg[i] = '\0';
    }

    /*
       Prefer extracting after "hex" if both display and hex exist.
    */
    start = find_marker_index(text, "hex");

    if (start >= 0) {
        start += 3;
    } else {
        start = find_marker_index(text, "display");

        if (start >= 0) {
            start += 7;
        } else {
            start = find_marker_index(text, "show");

            if (start >= 0) {
                start += 4;
            } else {
                start = find_marker_index(text, "message");

                if (start >= 0) {
                    start += 7;
                }
            }
        }
    }

    if (start < 0) {
        return;
    }

    while (text[start] == ' ') {
        start++;
    }

    /*
       Stop if another command begins after the HEX message.
       This prevents HEX from eating LED/speaker command text.
    */
    end = -1;
    end = min_positive_index(end, find_marker_index(text + start, " led module"));
    end = min_positive_index(end, find_marker_index(text + start, " module"));
    end = min_positive_index(end, find_marker_index(text + start, " speaker"));
    end = min_positive_index(end, find_marker_index(text + start, " frequency"));
    end = min_positive_index(end, find_marker_index(text + start, " turn on"));
    end = min_positive_index(end, find_marker_index(text + start, " rgb"));
    end = min_positive_index(end, find_marker_index(text + start, " spk"));

    if (end < 0) {
        end = strlen(text + start);
    }

    for (i = start; text[i] != '\0' && (i - start) < end && j < LOCAL_MSG_MAX - 1; i++) {
        out_msg[j++] = text[i];
    }

    out_msg[j] = '\0';

    while (j > 0 && out_msg[j - 1] == ' ') {
        out_msg[j - 1] = '\0';
        j--;
    }
}

/* =========================
   LED module decoder
   ========================= */

static uint32_t decode_led_module_color_mask(const char *text)
{
    uint32_t color_mask = LEDMOD_COLOR_NONE;

    if (contains_word(text, "red")) {
        color_mask |= LEDMOD_COLOR_RED;
    }

    if (contains_word(text, "green")) {
        color_mask |= LEDMOD_COLOR_GREEN;
    }

    if (contains_word(text, "yellow")) {
        color_mask |= LEDMOD_COLOR_YELLOW;
    }

    return color_mask;
}

static void decode_led_module(const char *text, uint32_t *target_mask, uint32_t *error_flags)
{
    uint32_t color_mask;
    uint32_t mode;
    int period_ms;

    if (!contains_word(text, "module") && !contains_word(text, "ledmodule")) {
        return;
    }

    color_mask = decode_led_module_color_mask(text);

    if (color_mask == LEDMOD_COLOR_NONE) {
        *error_flags |= ERR_MISSING_COLOR;
        color_mask = LEDMOD_COLOR_GREEN;
    }

    if (has_blink_mode(text)) {
        mode = MODE_BLINK;
        period_ms = find_period_ms(text);

        if (period_ms <= 0) {
            *error_flags |= ERR_MISSING_PERIOD;
            period_ms = 1000;
        }
    } else {
        mode = MODE_STATIC;
        period_ms = 0;
    }

    write_instr(LEDMOD_ENABLE, 1);
    write_instr(LEDMOD_COLOR_MASK, color_mask);
    write_instr(LEDMOD_MODE, mode);
    write_instr(LEDMOD_PERIOD_MS, (uint32_t)period_ms);

    *target_mask |= TARGET_LED_MODULE;
}

/* =========================
   FPGA LED decoder
   ========================= */

static void decode_fpga_led(const char *text, uint32_t *target_mask, uint32_t *error_flags)
{
    int led_count;
    int period_ms;
    uint32_t direction;
    uint32_t mode;

    /*
       If "module" exists, this is LED module, not FPGA LED.
    */
    if (contains_word(text, "module") || contains_word(text, "ledmodule")) {
        return;
    }

    /*
       FPGA LED needs led/turnon/turn on/left/right.
    */
    if (!contains_word(text, "led") &&
        !contains_word(text, "turnon") &&
        !contains_word(text, "turn on") &&
        !contains_word(text, "left") &&
        !contains_word(text, "right")) {
        return;
    }

    led_count = find_number_before_key(text, "led");

    if (led_count <= 0) {
        led_count = find_number_after_key(text, "turn on");
    }

    if (led_count <= 0) {
        led_count = find_number_after_key(text, "turnon");
    }

    if (led_count <= 0) {
        *error_flags |= ERR_MISSING_LED_COUNT;
        led_count = 1;
    }

    if (led_count > 10) {
        led_count = 10;
    }

    if (contains_word(text, "right")) {
        direction = DIR_RIGHT;
    } else if (contains_word(text, "left")) {
        direction = DIR_LEFT;
    } else {
        *error_flags |= ERR_MISSING_DIRECTION;
        direction = DIR_LEFT;
    }

    if (has_blink_mode(text)) {
        mode = MODE_BLINK;
        period_ms = find_period_ms(text);

        if (period_ms <= 0) {
            *error_flags |= ERR_MISSING_PERIOD;
            period_ms = 1000;
        }
    } else {
        mode = MODE_STATIC;
        period_ms = 0;
    }

    write_instr(FPGALED_ENABLE, 1);
    write_instr(FPGALED_COUNT, (uint32_t)led_count);
    write_instr(FPGALED_DIRECTION, direction);
    write_instr(FPGALED_MODE, mode);
    write_instr(FPGALED_PERIOD_MS, (uint32_t)period_ms);

    *target_mask |= TARGET_FPGA_LED;
}

/* =========================
   HEX decoder
   ========================= */

static void decode_hex_display(const char *text, uint32_t *target_mask, uint32_t *error_flags)
{
    char msg[LOCAL_MSG_MAX];

    if (!contains_word(text, "display") &&
        !contains_word(text, "hex") &&
        !contains_word(text, "show") &&
        !contains_word(text, "message")) {
        return;
    }

    extract_hex_message(text, msg);

    if (msg[0] == '\0') {
        *error_flags |= ERR_MISSING_HEX_MSG;
        strcpy(msg, "nomsg");
    }

    write_instr(HEX_ENABLE, 1);
    write_instr(HEX_MODE, MODE_SCROLL);
    write_hex_message(msg);

    *target_mask |= TARGET_HEX;
}

/* =========================
   Speaker decoder
   ========================= */

static void decode_speaker(const char *text, uint32_t *target_mask, uint32_t *error_flags)
{
    int freq;

    if (!contains_word(text, "speaker") &&
        !contains_word(text, "frequency") &&
        !contains_word(text, "hz") &&
        !contains_word(text, "tone") &&
        !contains_word(text, "sound")) {
        return;
    }

    freq = find_number_before_key(text, "hz");

    if (freq <= 0) {
        freq = find_number_after_key(text, "frequency");
    }

    if (freq <= 0) {
        freq = find_number_after_key(text, "speaker");
    }

    if (freq <= 0) {
        freq = find_largest_number(text);
    }

    if (freq <= 0) {
        *error_flags |= ERR_MISSING_FREQUENCY;
        freq = 1000;
    }

    /*
       Clamp to a practical range.
       You can change this depending on your speaker code.
    */
    if (freq < 50) {
        freq = 50;
    }

    if (freq > 50000) {
        freq = 50000;
    }

    write_instr(SPEAKER_ENABLE, 1);
    write_instr(SPEAKER_FREQ_HZ, (uint32_t)freq);
    write_instr(SPEAKER_DURATION_MS, 0);

    *target_mask |= TARGET_SPEAKER;
}

/* =========================
   Printing decoded result
   ========================= */

static const char *mode_to_string(uint32_t mode)
{
    if (mode == MODE_BLINK) return "blink";
    if (mode == MODE_SCROLL) return "scroll";
    return "static";
}

static const char *direction_to_string(uint32_t dir)
{
    if (dir == DIR_RIGHT) return "right";
    return "left";
}

static void print_led_module_colors(uint32_t mask)
{
    int first = 1;

    if (mask & LEDMOD_COLOR_RED) {
        printf("red");
        first = 0;
    }

    if (mask & LEDMOD_COLOR_GREEN) {
        if (!first) printf("+");
        printf("green");
        first = 0;
    }

    if (mask & LEDMOD_COLOR_YELLOW) {
        if (!first) printf("+");
        printf("yellow");
        first = 0;
    }

    if (first) {
        printf("none");
    }
}

static void print_hex_message_from_sdram(void)
{
    volatile char *msg = (volatile char *)(INSTRUCTION_BASE + HEX_MSG_BASE);
    int len = (int)read_instr(HEX_MSG_LEN);
    int i;

    for (i = 0; i < len && i < HEX_MSG_MAX_LEN; i++) {
        printf("%c", msg[i]);
    }
}

static void print_decoded_instruction(uint32_t target_mask, uint32_t error_flags)
{
    printf("\n========== DECODING DONE ==========\n");

    printf("CMD_VALID        = %lu\n", (unsigned long)read_instr(CMD_VALID));
    printf("CMD_TARGET_MASK  = 0x%02lX\n", (unsigned long)target_mask);
    printf("CMD_SEQUENCE_ID  = %lu\n", (unsigned long)read_instr(CMD_SEQUENCE_ID));
    printf("CMD_ERROR_FLAGS  = 0x%02lX\n", (unsigned long)error_flags);

    printf("\nInstruction says:\n");

    if (target_mask & TARGET_HEX) {
        printf("- HEX Display: scroll message \"");
        print_hex_message_from_sdram();
        printf("\"\n");
    }

    if (target_mask & TARGET_LED_MODULE) {
        uint32_t color_mask = read_instr(LEDMOD_COLOR_MASK);
        uint32_t mode = read_instr(LEDMOD_MODE);
        uint32_t period = read_instr(LEDMOD_PERIOD_MS);

        printf("- LED Module: ");
        print_led_module_colors(color_mask);
        printf(", mode=%s", mode_to_string(mode));

        if (mode == MODE_BLINK) {
            printf(", period=%lu ms", (unsigned long)period);
        }

        printf("\n");
    }

    if (target_mask & TARGET_FPGA_LED) {
        uint32_t count = read_instr(FPGALED_COUNT);
        uint32_t dir = read_instr(FPGALED_DIRECTION);
        uint32_t mode = read_instr(FPGALED_MODE);
        uint32_t period = read_instr(FPGALED_PERIOD_MS);

        printf("- FPGA LEDs: turn on %lu LEDs from %s, mode=%s",
               (unsigned long)count,
               direction_to_string(dir),
               mode_to_string(mode));

        if (mode == MODE_BLINK) {
            printf(", period=%lu ms", (unsigned long)period);
        }

        printf("\n");
    }

    if (target_mask & TARGET_SPEAKER) {
        uint32_t freq = read_instr(SPEAKER_FREQ_HZ);

        printf("- Speaker: frequency=%lu Hz\n", (unsigned long)freq);
    }

    if (target_mask == TARGET_NONE) {
        printf("- No known peripheral decoded\n");
    }

    printf("===================================\n\n");
    fflush(stdout);
}

/* =========================
   Public decoder functions
   ========================= */

void decoder_init(void)
{
    clear_instruction_registers();

    write_status(STATUS_CMD_READY, 0);
    write_status(STATUS_CMD_ACK, 0);
}

void decoder_decode_from_sdram(void)
{
    volatile char *src = (volatile char *)TEXT_BUFFER_BASE;
    char text[DECODER_TEXT_MAX];
    uint32_t target_mask = TARGET_NONE;
    uint32_t error_flags = ERR_NONE;
    int i;

    if (read_status(STATUS_TEXT_READY) == 0) {
        return;
    }

    for (i = 0; i < DECODER_TEXT_MAX; i++) {
        text[i] = '\0';
    }

    for (i = 0; i < DECODER_TEXT_MAX - 1; i++) {
        text[i] = src[i];

        if (src[i] == '\0') {
            break;
        }
    }

    text[DECODER_TEXT_MAX - 1] = '\0';

    printf("\nDecoder: raw SDRAM text = %s\n", text);

    normalize_text(text);

    printf("Decoder: normalized text = %s\n", text);
    fflush(stdout);

    clear_instruction_registers();

    /*
       Decode every possible peripheral.
       One sentence can activate multiple peripherals.
    */
    decode_hex_display(text, &target_mask, &error_flags);
    decode_led_module(text, &target_mask, &error_flags);
    decode_speaker(text, &target_mask, &error_flags);
    decode_fpga_led(text, &target_mask, &error_flags);

    if (target_mask == TARGET_NONE) {
        error_flags |= ERR_UNKNOWN_PERIPHERAL;
    }

    command_sequence++;

    write_instr(CMD_VALID, 1);
    write_instr(CMD_TARGET_MASK, target_mask);
    write_instr(CMD_SEQUENCE_ID, command_sequence);
    write_instr(CMD_ERROR_FLAGS, error_flags);

    /*
       Status update.
       Decoder consumed text and produced command registers.
    */
    write_status(STATUS_TEXT_READY, 0);
    write_status(STATUS_CMD_READY, 1);
    write_status(STATUS_CMD_ACK, 0);
    write_status(STATUS_LAST_ERROR, error_flags);

    print_decoded_instruction(target_mask, error_flags);
}
