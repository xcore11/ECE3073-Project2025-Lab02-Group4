#include "system.h"
#include "io.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "altera_avalon_pio_regs.h"
#include "sys/alt_irq.h"
#include "includes.h"
#include "control.h"
#include "soundeffects.h"

volatile int switch_state = 0;
volatile int key_state = 0;
volatile int GPIO_state = 0;

static uint32_t control_event_seq = 0;
static uint32_t control_switch_event_seq = 0;
static uint32_t last_rt_activity_seq = 0;
static int rt_busy_ticks = 0;
static int led_blink_tick = 0;
static int led_blink_visible = 0;
static uint32_t last_game_mode = 0xFFFFFFFFu;
static uint32_t last_debug_msg_seq = 0;
static char snake_action_history[DEBUG_CONTROL_MESSAGE_BYTES + 1];

#define HEX_SCROLL_STEP_MS              200u   /* HEX scroll speed. Smaller = faster. 100 ms is 2x faster than old 200 ms. */
#define LED_TASK_STEP_MS                50u
#define MENU_BLINK_TOGGLE_MS           500u   /* main-menu yellow blink toggles every 0.5 s */
#define WIN_BLINK_TOGGLE_MS            5000u  /* battleship win blink toggles every 5 s */

static uint32_t ms_to_os_ticks(uint32_t ms)
{
    uint32_t ticks = (uint32_t)(((uint64_t)ms * (uint64_t)OS_TICKS_PER_SEC + 999u) / 1000u);
    return (ticks == 0u) ? 1u : ticks;
}

/* ============================================================
   SDRAM helpers
   ============================================================ */
static void shared_write32(uint32_t offset, uint32_t value)
{
    IOWR_32DIRECT(SHARED_FLAGS_BASE, offset, value);
}

static uint32_t shared_read32(uint32_t offset)
{
    return IORD_32DIRECT(SHARED_FLAGS_BASE, offset);
}

static void debug_write32(uint32_t offset, uint32_t value)
{
    IOWR_32DIRECT(DEBUG_CONTROL_BASE, offset, value);
}

static uint32_t debug_read32(uint32_t offset)
{
    return IORD_32DIRECT(DEBUG_CONTROL_BASE, offset);
}

static uint8_t debug_read8(uint32_t offset)
{
    return (uint8_t)(IORD_8DIRECT(DEBUG_CONTROL_BASE, offset) & 0xFFu);
}

static void debug_write8(uint32_t offset, uint8_t value)
{
    IOWR_8DIRECT(DEBUG_CONTROL_BASE, offset, value);
}

static void read_debug_mailbox_message(char *dst, int max_len)
{
    int i;

    if (dst == NULL || max_len <= 0)
        return;

    for (i = 0; i < max_len - 1 && i < DEBUG_CONTROL_MESSAGE_BYTES; i++) {
        char c = (char)debug_read8(FLAG_CONTROL_MESSAGE + i);
        if (c == '\0')
            break;
        if (c < 32 || c > 126)
            c = ' ';
        dst[i] = c;
    }
    dst[i] = '\0';
}

static void clear_debug_mailbox_message(void)
{
    int i;
    for (i = 0; i < DEBUG_CONTROL_MESSAGE_BYTES; i++)
        debug_write8(FLAG_CONTROL_MESSAGE + i, 0);
}

/* ============================================================
   Output helpers
   ============================================================ */
static void set_all_hex_off(void)
{
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, 0xFF);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX1_BASE, 0xFF);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX2_BASE, 0xFF);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX3_BASE, 0xFF);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX4_BASE, 0xFF);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX5_BASE, 0xFF);
}

static void write_hex_pos(int pos, int seg)
{
    switch (pos) {
    case 0: IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, seg); break;
    case 1: IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX1_BASE, seg); break;
    case 2: IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX2_BASE, seg); break;
    case 3: IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX3_BASE, seg); break;
    case 4: IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX4_BASE, seg); break;
    case 5: IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX5_BASE, seg); break;
    default: break;
    }
}

static void set_ledr_value(uint32_t value)
{
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_LED_BASE, value & 0x3FFu);
}

static uint32_t make_ledr_bar(unsigned int count)
{
    uint32_t v = 0;
    unsigned int i;

    if (count > 10)
        count = 10;

    for (i = 0; i < count; i++)
        v |= (1u << i);

    return v & 0x3FFu;
}

static void set_led_module_bits(uint32_t bits)
{
    red_light((bits & LED_MODULE_RED_BIT) ? 1 : 0);
    yellow_light((bits & LED_MODULE_YELLOW_BIT) ? 1 : 0);
    green_light((bits & LED_MODULE_GREEN_BIT) ? 1 : 0);
}

static void outputs_all_off(void)
{
    set_all_hex_off();
    set_ledr_value(0);
    set_led_module_bits(0);
    play_speaker(0, 0);
}

/* ============================================================
   Shared input publishing
   ============================================================ */
uint32_t control_get_switch_state(void)
{
    return (uint32_t)(switch_state & CONTROL_SW_MASK);
}

uint32_t control_get_key_state(void)
{
    return (uint32_t)(key_state & CONTROL_KEY_MASK);
}

void control_shared_flags_init(void)
{
    int i;

    switch_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_SW_BASE) & CONTROL_SW_MASK;
    key_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_PB_BASE) & CONTROL_KEY_MASK;

    control_event_seq = 0;
    control_switch_event_seq = 0;
    snake_action_history[0] = '\0';

    shared_write32(FLAG_CONTROL_PROCESSOR_READY, 1);
    shared_write32(FLAG_CONTROL_EVENT_SEQ, control_event_seq);
    shared_write32(FLAG_CONTROL_KEY_STATE, (uint32_t)key_state);
    shared_write32(FLAG_CONTROL_KEY_PRESSED_MASK, 0);
    shared_write32(FLAG_CONTROL_SWITCH_STATE, (uint32_t)switch_state);
    shared_write32(FLAG_CONTROL_SWITCH_EVENT_SEQ, control_switch_event_seq);
    shared_write32(FLAG_CONTROL_LAST_EVENT_TYPE, CONTROL_EVENT_NONE);
    shared_write32(FLAG_CONTROL_LAST_EVENT_VALUE, 0);

    /* Clear one-shot SFX and exported status flags. */
    shared_write32(FLAG_SFX_EAT_APPLE, 0);
    shared_write32(FLAG_SFX_GAME_OVER, 0);
    shared_write32(FLAG_SFX_PORTAL, 0);
    shared_write32(FLAG_SFX_BATTLE_HIT, 0);
    shared_write32(FLAG_SFX_BATTLE_MISS, 0);
    shared_write32(FLAG_SFX_CHANGE_ARSENAL, 0);
    shared_write32(FLAG_SFX_MENU_BLIP, 0);
    shared_write32(FLAG_SFX_CLICK, 0);
    shared_write32(FLAG_SFX_ENTER_SNAKE, 0);
    shared_write32(FLAG_SFX_ENTER_DRAW, 0);
    shared_write32(FLAG_SFX_ENTER_BATTLE, 0);
    shared_write32(FLAG_SFX_CLEAR, 0);
    shared_write32(FLAG_SFX_SNAKE_TURN, 0);

    for (i = 0; i < DEBUG_CONTROL_MESSAGE_BYTES; i++)
        debug_write8(FLAG_CONTROL_MESSAGE + i, 0);
    debug_write32(FLAG_CONTROL_EVENT_SEQ, 0);
    debug_write32(FLAG_CONTROL_LAST_EVENT_TYPE, DEBUG_CONTROL_CMD_NONE);
    debug_write32(FLAG_CONTROL_LAST_EVENT_VALUE, 0);
    debug_write32(FLAG_CONTROL_SPEAKER_OPTION, 0);
    debug_write32(FLAG_CONTROL_LED_MODULE, 0);
    debug_write32(FLAG_CONTROL_LEDR, 0);

    outputs_all_off();
}

static void publish_control_event(uint32_t event_type, uint32_t event_value, uint32_t key_pressed_mask)
{
    control_event_seq++;

    shared_write32(FLAG_CONTROL_KEY_STATE, (uint32_t)(key_state & CONTROL_KEY_MASK));
    shared_write32(FLAG_CONTROL_KEY_PRESSED_MASK, key_pressed_mask & CONTROL_KEY_MASK);
    shared_write32(FLAG_CONTROL_SWITCH_STATE, (uint32_t)(switch_state & CONTROL_SW_MASK));
    shared_write32(FLAG_CONTROL_LAST_EVENT_TYPE, event_type);
    shared_write32(FLAG_CONTROL_LAST_EVENT_VALUE, event_value);

    if (event_type == CONTROL_EVENT_SWITCH) {
        control_switch_event_seq++;
        shared_write32(FLAG_CONTROL_SWITCH_EVENT_SEQ, control_switch_event_seq);
    }

    shared_write32(FLAG_CONTROL_EVENT_SEQ, control_event_seq);
}

static void notify_img_and_vga_processors(void)
{
#ifdef CON_IMG_IRQ_TX_BASE
    IOWR_ALTERA_AVALON_PIO_DATA(CON_IMG_IRQ_TX_BASE, 0x1);
#endif
#ifdef CON_VGA_IRQ_TX_BASE
    IOWR_ALTERA_AVALON_PIO_DATA(CON_VGA_IRQ_TX_BASE, 0x1);
#endif
#ifdef CON_IMG_IRQ_TX_BASE
    IOWR_ALTERA_AVALON_PIO_DATA(CON_IMG_IRQ_TX_BASE, 0x0);
#endif
#ifdef CON_VGA_IRQ_TX_BASE
    IOWR_ALTERA_AVALON_PIO_DATA(CON_VGA_IRQ_TX_BASE, 0x0);
#endif
}

static void switch_isr(void *context)
{
    (void)context;
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PIO_SW_BASE, CONTROL_SW_MASK);
    switch_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_SW_BASE) & CONTROL_SW_MASK;

    OSSemPost(input_update_sem);
}

void input_task(void *pdata)
{
    INT8U err;
    (void)pdata;

    while (1) {
        OSSemPend(input_update_sem, 0, &err);
		#ifdef PIO_GPIO_BASE
        	GPIO_state = GPIO_state & ~0x2;
			IOWR_ALTERA_AVALON_PIO_DATA(PIO_GPIO_BASE, GPIO_state);
		#endif
        publish_control_event(CONTROL_EVENT_SWITCH, (uint32_t)switch_state, 0);

        if ((~key_state) & CONTROL_KEY0_MASK) {
            publish_control_event(CONTROL_EVENT_KEY, CONTROL_KEY0_MASK, CONTROL_KEY0_MASK);
            key_state = (key_state | CONTROL_KEY0_MASK) & CONTROL_KEY_MASK;
        }

        if ((~key_state) & CONTROL_KEY1_MASK) {
            publish_control_event(CONTROL_EVENT_KEY, CONTROL_KEY1_MASK, CONTROL_KEY1_MASK);
            key_state = (key_state | CONTROL_KEY1_MASK) & CONTROL_KEY_MASK;
        }

        notify_img_and_vga_processors();
    }
}

void switch_setup(void)
{
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PIO_SW_BASE, CONTROL_SW_MASK);
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(PIO_SW_BASE, CONTROL_SW_MASK);

    alt_ic_isr_register(PIO_SW_IRQ_INTERRUPT_CONTROLLER_ID,
                        PIO_SW_IRQ,
                        switch_isr,
                        NULL,
                        NULL);

    switch_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_SW_BASE) & CONTROL_SW_MASK;
}

static void key_isr(void *context)
{
    (void)context;
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PIO_PB_BASE, CONTROL_KEY_MASK);
    key_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_PB_BASE) & CONTROL_KEY_MASK;

#ifdef PIO_GPIO_BASE
	GPIO_state = GPIO_state | 0x2;
	IOWR_ALTERA_AVALON_PIO_DATA(PIO_GPIO_BASE, GPIO_state);
#endif

    OSSemPost(input_update_sem);
}

void key_setup(void)
{
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PIO_PB_BASE, CONTROL_KEY_MASK);
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(PIO_PB_BASE, CONTROL_KEY_MASK);

    alt_ic_isr_register(PIO_PB_IRQ_INTERRUPT_CONTROLLER_ID,
                        PIO_PB_IRQ,
                        key_isr,
                        NULL,
                        NULL);

    key_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_PB_BASE) & CONTROL_KEY_MASK;
}

#ifdef CON_IMG_IRQ_RX_BASE
static void img_rx_isr(void *context)
{
    (void)context;
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(CON_IMG_IRQ_RX_BASE, 0x1);
    OSSemPost(leds_update_sem);
}
#endif

void img_rx_setup(void)
{
#ifdef CON_IMG_IRQ_RX_BASE
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(CON_IMG_IRQ_RX_BASE, 0x1);
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(CON_IMG_IRQ_RX_BASE, 0x1);
    alt_ic_isr_register(CON_IMG_IRQ_RX_IRQ_INTERRUPT_CONTROLLER_ID,
                        CON_IMG_IRQ_RX_IRQ,
                        img_rx_isr,
                        NULL,
                        NULL);
#endif
}

#ifdef CON_VGA_IRQ_RX_BASE
static void vga_rx_isr(void *context)
{
    (void)context;
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(CON_VGA_IRQ_RX_BASE, 0x1);
    OSSemPost(leds_update_sem);
}
#endif

void vga_rx_setup(void)
{
#ifdef CON_VGA_IRQ_RX_BASE
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(CON_VGA_IRQ_RX_BASE, 0x1);
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(CON_VGA_IRQ_RX_BASE, 0x1);
    alt_ic_isr_register(CON_VGA_IRQ_RX_IRQ_INTERRUPT_CONTROLLER_ID,
                        CON_VGA_IRQ_RX_IRQ,
                        vga_rx_isr,
                        NULL,
                        NULL);
#endif
}

/* ============================================================
   HEX display task
   ============================================================ */
static const char *menu_name_from_index(uint32_t menu)
{
    if (menu == MENU_SELECT_BATTLE) return "MENU BATTLESHIP";
    if (menu == MENU_SELECT_SNAKE) return "MENU SNAKE";
    if (menu == MENU_SELECT_DRAW) return "MENU DRAW";
    if (menu == MENU_SELECT_DEBUG) return "MENU DEBUG";
    return "MENU";
}

static int string_is_action(const char *s)
{
    /* User requested snake HEX history to show pickups/status only.
       Do not capture movement directions anymore. */
    return (strcmp(s, "APPLE") == 0 || strcmp(s, "PORTAL") == 0 ||
            strcmp(s, "PORTA") == 0 || strcmp(s, "PORTB") == 0 ||
            strcmp(s, "DEAD") == 0);
}

static void append_snake_action(const char *msg)
{
    int cur_len;
    int add_len;

    if (msg == NULL || msg[0] == '\0')
        return;

    if (strcmp(msg, "DEAD") == 0) {
        strcpy(snake_action_history, "DEAD");
        return;
    }

    if (strcmp(msg, "PORTA") == 0 || strcmp(msg, "PORTB") == 0)
        msg = "PORTAL";

    if (!string_is_action(msg))
        return;

    cur_len = strlen(snake_action_history);
    add_len = strlen(msg);

    if (cur_len == 0) {
        strncpy(snake_action_history, msg, DEBUG_CONTROL_MESSAGE_BYTES);
        snake_action_history[DEBUG_CONTROL_MESSAGE_BYTES] = '\0';
        return;
    }

    if (cur_len + 1 + add_len >= DEBUG_CONTROL_MESSAGE_BYTES) {
        int remove = (cur_len + 1 + add_len) - (DEBUG_CONTROL_MESSAGE_BYTES - 1);
        if (remove < cur_len) {
            memmove(snake_action_history, snake_action_history + remove, cur_len - remove + 1);
            cur_len = strlen(snake_action_history);
        }
    }

    if (cur_len > 0 && cur_len < DEBUG_CONTROL_MESSAGE_BYTES - 1)
        strcat(snake_action_history, " ");

    strncat(snake_action_history, msg,
            DEBUG_CONTROL_MESSAGE_BYTES - strlen(snake_action_history));
}

static void update_message_history(uint32_t game)
{
    uint32_t seq = debug_read32(FLAG_CONTROL_EVENT_SEQ);
    char msg[DEBUG_CONTROL_MESSAGE_BYTES + 1];

    if (game != last_game_mode) {
        snake_action_history[0] = '\0';
        last_game_mode = game;
    }

    if (seq == last_debug_msg_seq)
        return;

    last_debug_msg_seq = seq;
    read_debug_mailbox_message(msg, sizeof(msg));

    if (game == GAME_MODE_SNAKE)
        append_snake_action(msg);
}

static void build_scroll_message(char *dst, int dst_len, uint32_t game)
{
    char mailbox[DEBUG_CONTROL_MESSAGE_BYTES + 1];
    uint32_t menu;
    uint32_t battle_win;
    uint32_t snake_state;

    if (dst == NULL || dst_len <= 0)
        return;

    mailbox[0] = '\0';
    read_debug_mailbox_message(mailbox, sizeof(mailbox));

    if (game == GAME_MODE_MENU) {
        menu = shared_read32(FLAG_CURRENT_MENU);
        snprintf(dst, dst_len, "%s", menu_name_from_index(menu));
    }
    else if (game == GAME_MODE_SNAKE) {
        snake_state = shared_read32(SNAKE_FLAG_GAME_STATE);
        if (snake_state == SNAKE_STATE_LOST)
            snprintf(dst, dst_len, "DEAD");
        else if (snake_action_history[0] != '\0')
            snprintf(dst, dst_len, "%s", snake_action_history);
        else
            snprintf(dst, dst_len, "SNAKE");
    }
    else if (game == GAME_MODE_BATTLE) {
        uint32_t cross_left = shared_read32(FLAG_BATTLE_CROSS_LEFT);
        uint32_t square_left = shared_read32(FLAG_BATTLE_SQUARE_LEFT);
        uint32_t selected = shared_read32(FLAG_BATTLE_SELECTED_BOMB);

        /* Clamp stale/uninitialized SDRAM values. A 5-digit number here means
           Control read an old value before VGA published battle status. */
        if (cross_left > 99u) cross_left = 0u;
        if (square_left > 99u) square_left = 0u;

        battle_win = shared_read32(FLAG_BATTLE_WIN);
        if (battle_win) {
            snprintf(dst, dst_len, "WIN");
        } else {
            const char *sel = "STD";
            if (selected == 1u) sel = "CROSS";
            else if (selected == 2u) sel = "SQUARE";

            snprintf(dst, dst_len, "ARS %s CR%lu SQ%lu",
                     sel,
                     (unsigned long)cross_left,
                     (unsigned long)square_left);
        }
    }
    else if (game == GAME_MODE_DRAW) {
        snprintf(dst, dst_len, "R%lu G%lu B%lu Y%lu K%lu W%lu",
                 (unsigned long)shared_read32(FLAG_DRAW_RED_COUNT),
                 (unsigned long)shared_read32(FLAG_DRAW_GREEN_COUNT),
                 (unsigned long)shared_read32(FLAG_DRAW_BLUE_COUNT),
                 (unsigned long)shared_read32(FLAG_DRAW_YELLOW_COUNT),
                 (unsigned long)shared_read32(FLAG_DRAW_BLACK_COUNT),
                 (unsigned long)shared_read32(FLAG_DRAW_WHITE_COUNT));
    }
    else if (game == GAME_MODE_DEBUG || shared_read32(FLAG_DEBUG_MODE) != 0) {
        char dbg_msg[DEBUG_CONTROL_MESSAGE_BYTES + 1];
        debug_control_copy_message(dbg_msg, sizeof(dbg_msg));
        if (dbg_msg[0] != '\0')
            snprintf(dst, dst_len, "%s", dbg_msg);
        else if (mailbox[0] != '\0')
            snprintf(dst, dst_len, "%s", mailbox);
        else
            snprintf(dst, dst_len, "DEBUG");
    }
    else {
        snprintf(dst, dst_len, "READY");
    }
}

#define UTIL_HISTORY_SIZE 10 // buffer size
static int utilisation_history[UTIL_HISTORY_SIZE] = {0};
static int utilisation_history_index = 0;
static int current_system_load = 0;

static void write_load_to_last_two(void)
{
    int load = current_system_load;
    if (load < 0) load = 0;
    if (load > 99) load = 99;

    write_hex_pos(1, translator((char)('0' + (load / 10))));
    write_hex_pos(0, translator((char)('0' + (load % 10))));
}

static void write_cpu_to_last_two(void)
{
    extern INT8U OSCPUUsage;
    int cpu = (int)OSCPUUsage;

    if (cpu < 0) cpu = 0;
    if (cpu > 99) cpu = 99;

    write_hex_pos(1, translator((char)('0' + (cpu / 10))));
    write_hex_pos(0, translator((char)('0' + (cpu % 10))));
}

static void write_scroll_window(const char *msg, int scroll_offset, int chars_available)
{
    char padded[128];
    int len;
    int i;
    int start_pos;

    if (msg == NULL || msg[0] == '\0')
        msg = " ";

    snprintf(padded, sizeof(padded), "      %s      ", msg);
    len = strlen(padded);
    if (len <= 0)
        len = 1;

    start_pos = scroll_offset % len;

    for (i = 0; i < chars_available; i++) {
        int src = start_pos + i;
        char c;
        if (src >= len)
            src -= len;
        c = padded[src];

        /* chars_available==4 writes HEX5..HEX2. chars_available==6 writes HEX5..HEX0. */
        write_hex_pos(5 - i, translator(c));
    }
}

void HEX_task(void *pdata)
{
    uint32_t game;
    char msg[DEBUG_CONTROL_MESSAGE_BYTES + 1];
    char last_msg[DEBUG_CONTROL_MESSAGE_BYTES + 1];
    int scroll_offset = 0;
    uint32_t last_game_for_scroll = 0xFFFFFFFFu;

    extern INT8U OSCPUUsage;
	static int load_timer = 0;

    (void)pdata;
    last_msg[0] = '\0';

    while (1) {
        uint32_t sw = (uint32_t)(switch_state & CONTROL_SW_MASK);
        int hex_master = (sw & CONTROL_SW1_HEX_MASTER) != 0;
        int scroll_master = (sw & CONTROL_SW2_SCROLL_MASTER) != 0;
        int cpu_master = (sw & CONTROL_SW3_CPU_MASTER) != 0;
        int cpu = (int)OSCPUUsage;

        load_timer++;
		if (load_timer >= 10) {

			if (cpu > 99) cpu = 99;
			if (cpu < 0) cpu = 0;

			// Add to rolling history buffer
			utilisation_history[utilisation_history_index] = cpu;
			utilisation_history_index++;
			if (utilisation_history_index >= UTIL_HISTORY_SIZE) {
				utilisation_history_index = 0; // Loop back around
			}

			// Average the buffer
			int sum = 0;
			for (int i = 0; i < UTIL_HISTORY_SIZE; i++) {
				sum += utilisation_history[i];
			}
			current_system_load = sum / UTIL_HISTORY_SIZE;

			load_timer = 0;
		}

        game = shared_read32(FLAG_CURRENT_GAME);
        update_message_history(game);

        if (!hex_master) {
            set_all_hex_off();
            scroll_offset = 0;
            OSTimeDlyHMSM(0, 0, 0, 100);
            continue;
        }

        build_scroll_message(msg, sizeof(msg), game);

        if (game != last_game_for_scroll) {
            last_game_for_scroll = game;
            scroll_offset = 0;
            last_msg[0] = '\0';
        }

        if (strcmp(msg, last_msg) != 0) {
            /* Keep scrolling smoothly when values update, for example draw
               pixel counts, snake APPLE/PORTAL history, or battle arsenal.
               Only reset the scroll when the game/mode changes. */
            strncpy(last_msg, msg, sizeof(last_msg));
            last_msg[sizeof(last_msg) - 1] = '\0';
        }

        if (scroll_master) {
			write_scroll_window(msg, scroll_offset, 4);
			write_load_to_last_two();
            scroll_offset++;
        }
        else if (cpu_master){
            /* HEX master can be on while scrolling is off. Only CPU may use HEX1..0. */
            write_hex_pos(5, 0xFF);
            write_hex_pos(4, 0xFF);
            write_hex_pos(3, 0xFF);
            write_hex_pos(2, 0xFF);
            write_cpu_to_last_two();
            }
        else {
        		write_hex_pos(5, 0xFF);
				write_hex_pos(4, 0xFF);
				write_hex_pos(3, 0xFF);
				write_hex_pos(2, 0xFF);
                write_hex_pos(1, 0xFF);
                write_hex_pos(0, 0xFF);
            }
        OSTimeDlyHMSM(0, 0, 0, HEX_SCROLL_STEP_MS);
    }
}

/* ============================================================
   LED module + LEDR task
   ============================================================ */
void leds_update_task(void *pdata)
{
    uint32_t last_game = 0xFFFFFFFFu;
    int debug_was_active = 0;

    (void)pdata;

    while (1) {
        uint32_t game = shared_read32(FLAG_CURRENT_GAME);
        uint32_t rt_seq = shared_read32(FLAG_RT_ACTIVITY_SEQ);
        uint32_t led_module = 0;
        uint32_t ledr = 0;
        int blink_fast = 0;
        int blink_slow = 0;

        if (game != last_game) {
            led_blink_tick = (int)OSTimeGet();
            led_blink_visible = 0;
            last_game = game;
        }

        if (rt_seq != last_rt_activity_seq) {
            last_rt_activity_seq = rt_seq;
            rt_busy_ticks = 12;  /* about 600 ms at 50 ms update rate */
        } else if (rt_busy_ticks > 0) {
            rt_busy_ticks--;
        }

        if (game == GAME_MODE_DEBUG || shared_read32(FLAG_DEBUG_MODE) != 0) {
            debug_was_active = 1;
            debug_control_update();
            OSTimeDlyHMSM(0, 0, 0, LED_TASK_STEP_MS);
            continue;
        }

        if (debug_was_active) {
            debug_was_active = 0;
            debug_control_init();
            clear_debug_mailbox_message();
        }

        if (game == GAME_MODE_MENU) {
            blink_fast = 1;
            led_module = LED_MODULE_YELLOW_BIT;
            ledr = 0;
        }
        else if (game == GAME_MODE_SNAKE && shared_read32(SNAKE_FLAG_GAME_STATE) == SNAKE_STATE_LOST) {
            blink_fast = 1;
            led_module = LED_MODULE_RED_BIT | LED_MODULE_YELLOW_BIT | LED_MODULE_GREEN_BIT;
            ledr = 0x3FFu;
        }
        else if (game == GAME_MODE_BATTLE && shared_read32(FLAG_BATTLE_WIN) != 0) {
            blink_slow = 1; /* requested ship win blink every 5 seconds */
            led_module = LED_MODULE_RED_BIT | LED_MODULE_YELLOW_BIT | LED_MODULE_GREEN_BIT;
            ledr = 0x3FFu;
        }
        else {
            led_module = (rt_busy_ticks > 0) ? LED_MODULE_RED_BIT : LED_MODULE_GREEN_BIT;

            if (game == GAME_MODE_SNAKE) {
                ledr = make_ledr_bar((unsigned int)shared_read32(SNAKE_FLAG_APPLE_COUNT));
            }
            else if (game == GAME_MODE_BATTLE) {
                uint32_t loaded = shared_read32(FLAG_BATTLE_LOADED_SHIP_CELLS);
                uint32_t destroyed = shared_read32(FLAG_BATTLE_DESTROYED_CELLS);
                uint32_t remaining = (loaded > destroyed) ? (loaded - destroyed) : 0;
                ledr = make_ledr_bar((unsigned int)remaining);
            }
            else {
                ledr = 0;
            }
        }

        if (blink_fast || blink_slow) {
            uint32_t now_ticks = (uint32_t)OSTimeGet();
            uint32_t interval_ticks = blink_slow ?
                                      ms_to_os_ticks(WIN_BLINK_TOGGLE_MS) :
                                      ms_to_os_ticks(MENU_BLINK_TOGGLE_MS);

            if ((uint32_t)(now_ticks - (uint32_t)led_blink_tick) >= interval_ticks) {
                led_blink_tick = (int)now_ticks;
                led_blink_visible = !led_blink_visible;
            }
            if (!led_blink_visible) {
                led_module = 0;
                ledr = 0;
            }
        }

        set_led_module_bits(led_module);
        set_ledr_value(ledr);

        OSTimeDlyHMSM(0, 0, 0, LED_TASK_STEP_MS);
    }
}

/* ============================================================
   Speaker/SFX task
   ============================================================ */
static int consume_sfx_flag(uint32_t offset)
{
    if (shared_read32(offset) == 0)
        return 0;
    shared_write32(offset, 0);
    return 1;
}

static void clear_all_sfx_flags(void)
{
    shared_write32(FLAG_SFX_EAT_APPLE, 0);
    shared_write32(FLAG_SFX_GAME_OVER, 0);
    shared_write32(FLAG_SFX_PORTAL, 0);
    shared_write32(FLAG_SFX_BATTLE_HIT, 0);
    shared_write32(FLAG_SFX_BATTLE_MISS, 0);
    shared_write32(FLAG_SFX_CHANGE_ARSENAL, 0);
    shared_write32(FLAG_SFX_MENU_BLIP, 0);
    shared_write32(FLAG_SFX_CLICK, 0);
    shared_write32(FLAG_SFX_ENTER_SNAKE, 0);
    shared_write32(FLAG_SFX_ENTER_DRAW, 0);
    shared_write32(FLAG_SFX_ENTER_BATTLE, 0);
    shared_write32(FLAG_SFX_CLEAR, 0);
    shared_write32(FLAG_SFX_SNAKE_TURN, 0);
}

void sfx_task(void *pdata)
{
    (void)pdata;

    while (1) {
        uint32_t sw = (uint32_t)(switch_state & CONTROL_SW_MASK);
        int speaker_master = (sw & CONTROL_SW4_SPEAKER_MASTER) != 0;
        uint32_t game = shared_read32(FLAG_CURRENT_GAME);

        if (game == GAME_MODE_DEBUG || shared_read32(FLAG_DEBUG_MODE) != 0) {
            /* In DEBUG, SW4 gates the decoded speaker command. Gameplay SFX
               are disabled here, but a real "SET SPEAKER ... HZ" command
               runs continuously until DEBUG exits or SW4 turns off.

               Keep this delay short: debug_control_service_speaker() generates
               a small busy-wait tone slice, and long RTOS sleeps between slices
               make the speaker appear silent/choppy. */
            clear_all_sfx_flags();
            debug_control_service_speaker(speaker_master);
            OSTimeDlyHMSM(0, 0, 0, 1);
            continue;
        }

        if (!speaker_master) {
            clear_all_sfx_flags();
            play_speaker(0, 0);
            OSTimeDlyHMSM(0, 0, 0, 20);
            continue;
        }

        /* Short UI feedback first, then game-entry tunes, then gameplay events. */
        if (consume_sfx_flag(FLAG_SFX_CLICK)) {
            sfx_click();
        }
        else if (consume_sfx_flag(FLAG_SFX_SNAKE_TURN)) {
            sfx_snake_turn();
        }
        else if (consume_sfx_flag(FLAG_SFX_CLEAR)) {
            sfx_clear();
        }
        else if (consume_sfx_flag(FLAG_SFX_ENTER_SNAKE)) {
            sfx_enter_snake();
        }
        else if (consume_sfx_flag(FLAG_SFX_ENTER_DRAW)) {
            sfx_enter_draw();
        }
        else if (consume_sfx_flag(FLAG_SFX_ENTER_BATTLE)) {
            sfx_enter_battle();
        }
        else if (consume_sfx_flag(FLAG_SFX_PORTAL)) {
            sfx_portal();
        }
        else if (consume_sfx_flag(FLAG_SFX_EAT_APPLE)) {
            sfx_eat_apple();
        }
        else if (consume_sfx_flag(FLAG_SFX_GAME_OVER)) {
            sfx_end_screen();
        }
        else if (consume_sfx_flag(FLAG_SFX_BATTLE_HIT)) {
            sfx_explosion();
        }
        else if (consume_sfx_flag(FLAG_SFX_BATTLE_MISS)) {
            sfx_error_buzz();
        }
        else if (consume_sfx_flag(FLAG_SFX_CHANGE_ARSENAL)) {
            sfx_change();
        }
        else if (consume_sfx_flag(FLAG_SFX_MENU_BLIP)) {
            sfx_menu_blip();
        }
        else {
            play_speaker(0, 0);
        }

        OSTimeDlyHMSM(0, 0, 0, 1);
    }
}

void handle_switch4(void)
{
    /* SW4 is now only a speaker master enable. It does not start a default tone. */
    if ((switch_state & CONTROL_SW4_SPEAKER_MASTER) == 0)
        play_speaker(0, 0);
}

int translator(char a)
{
    if (a >= '0' && a <= '9') {
        const int num_table[10] = {
            0xC0, 0xF9, 0xA4, 0xB0, 0x99,
            0x92, 0x82, 0xF8, 0x80, 0x90
        };
        return num_table[a - '0'];
    }

    if (a >= 'a' && a <= 'z')
        a = (char)(a - 'a' + 'A');

    if (a >= 'A' && a <= 'Z') {
        const int alpha_table[26] = {
            0x88, 0x83, 0xC6, 0xA1, 0x86, 0x8E, 0x90, 0x89, 0xF9, 0xF1,
            0x8A, 0xC7, 0xC8, 0xAB, 0xC0, 0x8C, 0x98, 0xAF, 0x92, 0x87,
            0xC1, 0xE3, 0x81, 0x89, 0x91, 0xA4
        };
        return alpha_table[a - 'A'];
    }

    if (a == ' ' || a == '-' || a == '_')
        return 0xFF;

    return 0xFF;
}
