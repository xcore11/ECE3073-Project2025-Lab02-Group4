#include "system.h"
#include "altera_avalon_pio_regs.h"
#include "sys/alt_irq.h"
#include <stdio.h>
#include <string.h>
<<<<<<< Updated upstream
#include <unistd.h>
#include <stdlib.h>
#include "control.h"
#include "soundeffects.h"
=======
#include <io.h> // REQUIRED for IORD_32DIRECT / IOWR_32DIRECT
#include "control.h"
#include "countdown.h"
>>>>>>> Stashed changes

volatile int switch_state = 0;
volatile int key_state = 0;
volatile int GPIO_state = 0;
static int HEX_enable_bit = 0;
static int scroll_counter = 0;
static int scroll_offset = 0;
const int scroll_speed = 2000;

#define CON_IMG_IRQ_RX_IRQ_INTERRUPT_CONTROLLER_ID 0
#define CON_IMG_IRQ_RX_BASE 0x8011120
#define CON_VGA_IRQ_RX_IRQ_INTERRUPT_CONTROLLER_ID 0
#define CON_VGA_IRQ_RX_BASE 0x8011100
#define CON_IMG_IRQ_TX_IRQ_INTERRUPT_CONTROLLER_ID -1
#define CON_IMG_IRQ_TX_BASE 0x8011130
#define CON_VGA_IRQ_TX_BASE 0x8011110
#define CON_IMG_IRQ_RX_IRQ 3
#define CON_VGA_IRQ_RX_IRQ 4

<<<<<<< Updated upstream
// Forward Declarations / Prototypes
int translator(char a);

/* ========================================================================
   SNAKE GAME STATE MACHINE & SHARED MEMORY CONFIGURATION
   ======================================================================== */
#define STATE_IDLE        0
#define STATE_COUNTDOWN  1
#define STATE_GAME_LIVE  2

// Shared Mailbox Slots for Button Presses & Switch States
#define FLAG_CONTROL_KEY_PRESSED_MASK   0x808
#define FLAG_CONTROL_SWITCH_STATE       0x80C
#define FLAG_CONTROL_LAST_EVENT_TYPE    0x814

#define CONTROL_EVENT_KEY                1
#define CONTROL_EVENT_SWITCH             2

#define SHARED_GAME_STATE_PTR ((volatile uint32_t *)(NEW_SDRAM_CONTROLLER_0_BASE + 0x4000))

static int internal_game_state = STATE_IDLE;
static int countdown_val = 3;
static int countdown_tick = 0;
const int countdown_speed = 30000;

#define SHARED_FLAGS_BASE              0x05212000
#define FLAG_CURRENT_GAME              0x18
#define FLAG_GAME_RUNNING              0x1C
#define FLAG_SYSTEM_POWER              0x40

#define GAME_MODE_MENU                  0
#define GAME_MODE_SNAKE                 1

extern int run_synchronized_countdown(void);


/* ========================================================================
=======
#define SYSTEM_FLAGS_BASE              0x05212000
#define FLAG_CURRENT_GAME              0x18
#define FLAG_GAME_RUNNING              0x1C
#define FLAG_SYSTEM_POWER              0x40

#define GAME_MODE_MENU                 0
#define GAME_MODE_SNAKE                1

int game_already_started = 0;

/* Stub added to prevent linker errors based on main.c */
void control_shared_flags_init(void) {
    IOWR_32DIRECT(SHARED_FLAGS_BASE, FLAG_CONTROL_PROCESSOR_READY, 1);
}

/* ========================================================================
>>>>>>> Stashed changes
   INTERRUPT SERVICE ROUTINES & SETUP
   ======================================================================== */

static void switch_isr(void* context) {
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PIO_SW_BASE, 0x3FF);
    switch_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_SW_BASE);
<<<<<<< Updated upstream
=======

    // Broadcast switch states
    IOWR_32DIRECT(SYSTEM_FLAGS_BASE, FLAG_CONTROL_SWITCH_STATE, switch_state);
    IOWR_32DIRECT(SYSTEM_FLAGS_BASE, FLAG_CONTROL_KEY_PRESSED_MASK, 0x00); // Wipe phantom keys
    IOWR_32DIRECT(SYSTEM_FLAGS_BASE, FLAG_CONTROL_LAST_EVENT_TYPE, CONTROL_EVENT_SWITCH);

    // Force SDRAM write flush before pulsing IRQ
    IORD_32DIRECT(SYSTEM_FLAGS_BASE, FLAG_CONTROL_LAST_EVENT_TYPE);

    // Pulse the VGA IRQ line
    // (Using a safe dummy loop instead of usleep since we are inside an ISR)
    IOWR_ALTERA_AVALON_PIO_DATA(CON_VGA_IRQ_TX_BASE, 0x1);
    for(volatile int i = 0; i < 500; i++);
    IOWR_ALTERA_AVALON_PIO_DATA(CON_VGA_IRQ_TX_BASE, 0x0);
>>>>>>> Stashed changes
}

void switch_setup(void) {
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PIO_SW_BASE, 0x3FF);
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(PIO_SW_BASE, 0x3FF);
<<<<<<< Updated upstream

    alt_ic_isr_register(
        PIO_SW_IRQ_INTERRUPT_CONTROLLER_ID,
        PIO_SW_IRQ,
        switch_isr,
        NULL,
        NULL
    );

    switch_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_SW_BASE);
=======
    alt_ic_isr_register(PIO_SW_IRQ_INTERRUPT_CONTROLLER_ID, PIO_SW_IRQ, switch_isr, NULL, NULL);
    switch_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_SW_BASE);

    // >>> CRITICAL FIX: Sync initial switch states on startup <<<
    IOWR_32DIRECT(SYSTEM_FLAGS_BASE, FLAG_CONTROL_SWITCH_STATE, switch_state);
>>>>>>> Stashed changes
}

static void key_isr(void* context) {
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PIO_PB_BASE, 0x3);
    key_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_PB_BASE);
<<<<<<< Updated upstream
=======

    // >>> CRITICAL FIX: Broadcast key presses to the VGA Processor! <<<
    IOWR_32DIRECT(SYSTEM_FLAGS_BASE, FLAG_CONTROL_KEY_STATE, key_state);
    IOWR_32DIRECT(SYSTEM_FLAGS_BASE, FLAG_CONTROL_LAST_EVENT_TYPE, CONTROL_EVENT_KEY);
>>>>>>> Stashed changes
}

void key_setup(void) {
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PIO_PB_BASE, 0x3);
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(PIO_PB_BASE, 0x3);
<<<<<<< Updated upstream

    alt_ic_isr_register(
        PIO_PB_IRQ_INTERRUPT_CONTROLLER_ID,
        PIO_PB_IRQ,
        key_isr,
        NULL,
        NULL
    );

    key_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_PB_BASE);
=======
    alt_ic_isr_register(PIO_PB_IRQ_INTERRUPT_CONTROLLER_ID, PIO_PB_IRQ, key_isr, NULL, NULL);
    key_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_PB_BASE);

    // >>> CRITICAL FIX: Sync initial key states on startup <<<
    IOWR_32DIRECT(SYSTEM_FLAGS_BASE, FLAG_CONTROL_KEY_STATE, key_state);
>>>>>>> Stashed changes
}
static void img_rx_isr(void* context) {
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(CON_IMG_IRQ_RX_BASE, 0x1);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_GPIO_BASE, (GPIO_state & 0x2));
    GPIO_state = GPIO_state & 0x2;
}

void img_rx_setup(void) {
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(CON_IMG_IRQ_RX_BASE, 0x1);
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(CON_IMG_IRQ_RX_BASE, 0x1);
<<<<<<< Updated upstream

    alt_ic_isr_register(
        CON_IMG_IRQ_RX_IRQ_INTERRUPT_CONTROLLER_ID,
        CON_IMG_IRQ_RX_IRQ,
        img_rx_isr,
        NULL,
        NULL
    );
=======
    alt_ic_isr_register(CON_IMG_IRQ_RX_IRQ_INTERRUPT_CONTROLLER_ID, CON_IMG_IRQ_RX_IRQ, img_rx_isr, NULL, NULL);
>>>>>>> Stashed changes
}

static void vga_rx_isr(void* context) {
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(CON_VGA_IRQ_RX_BASE, 0x1);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_GPIO_BASE, (GPIO_state & 0x1));
    GPIO_state = GPIO_state & 0x1;
}

void vga_rx_setup(void) {
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(CON_VGA_IRQ_RX_BASE, 0x1);
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(CON_VGA_IRQ_RX_BASE, 0x1);
<<<<<<< Updated upstream

    alt_ic_isr_register(
        CON_VGA_IRQ_RX_IRQ_INTERRUPT_CONTROLLER_ID,
        CON_VGA_IRQ_RX_IRQ,
        vga_rx_isr,
        NULL,
        NULL
    );
=======
    alt_ic_isr_register(CON_VGA_IRQ_RX_IRQ_INTERRUPT_CONTROLLER_ID, CON_VGA_IRQ_RX_IRQ, vga_rx_isr, NULL, NULL);
>>>>>>> Stashed changes
}

/* ========================================================================
   PERIPHERAL HANDLER FUNCTIONS
   ======================================================================== */

<<<<<<< Updated upstream
void handle_key1(void)
{
    if ((~key_state) & 0x01)
    {
        IOWR_32DIRECT(SHARED_FLAGS_BASE, FLAG_CONTROL_KEY_PRESSED_MASK, 0x01);
        IOWR_32DIRECT(SHARED_FLAGS_BASE, FLAG_CONTROL_LAST_EVENT_TYPE, CONTROL_EVENT_KEY);

        IOWR_ALTERA_AVALON_PIO_DATA(CON_IMG_IRQ_TX_BASE, 0x1);
        IOWR_ALTERA_AVALON_PIO_DATA(CON_VGA_IRQ_TX_BASE, 0x1);
        IOWR_ALTERA_AVALON_PIO_DATA(CON_IMG_IRQ_TX_BASE, 0x0);
        IOWR_ALTERA_AVALON_PIO_DATA(CON_VGA_IRQ_TX_BASE, 0x0);

        GPIO_state = GPIO_state | 0x1;
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_GPIO_BASE, GPIO_state);

        key_state = (key_state | 0x01);
    }
}

void handle_key2(void)
{
    if ((~key_state) & 0x02)
    {
        IOWR_32DIRECT(SHARED_FLAGS_BASE, FLAG_CONTROL_KEY_PRESSED_MASK, 0x02);
        IOWR_32DIRECT(SHARED_FLAGS_BASE, FLAG_CONTROL_LAST_EVENT_TYPE, CONTROL_EVENT_KEY);

        IOWR_ALTERA_AVALON_PIO_DATA(CON_VGA_IRQ_TX_BASE, 0x1);
        IOWR_ALTERA_AVALON_PIO_DATA(CON_VGA_IRQ_TX_BASE, 0x0);

        GPIO_state = GPIO_state | 0x2;
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_GPIO_BASE, GPIO_state);

        key_state = (key_state | 0x02);
=======
void handle_key1(void) {
    if ((~key_state) & CONTROL_KEY0_MASK) {
        // 1. Tell the VGA processor exactly WHICH key was pressed
        IOWR_32DIRECT(SYSTEM_FLAGS_BASE, FLAG_CONTROL_KEY_PRESSED_MASK, CONTROL_KEY0_MASK);
        IOWR_32DIRECT(SYSTEM_FLAGS_BASE, FLAG_CONTROL_LAST_EVENT_TYPE, CONTROL_EVENT_KEY);

        // >>> CRITICAL FIX 1: The Memory Barrier <<<
        // Read the memory back to force the Avalon Switch Fabric to flush the
        // SDRAM write buffer BEFORE we pulse the hardware IRQ line.
        IORD_32DIRECT(SYSTEM_FLAGS_BASE, FLAG_CONTROL_LAST_EVENT_TYPE);

        // >>> CRITICAL FIX 2: Pulse Widening <<<
        // Hold the IRQ line high for a fraction of a millisecond so the
        // VGA processor's edge-capture register absolutely doesn't miss it.
        IOWR_ALTERA_AVALON_PIO_DATA(CON_IMG_IRQ_TX_BASE, 0x1);
        IOWR_ALTERA_AVALON_PIO_DATA(CON_VGA_IRQ_TX_BASE, 0x1);

        usleep(100);

        IOWR_ALTERA_AVALON_PIO_DATA(CON_IMG_IRQ_TX_BASE, 0x0);
        IOWR_ALTERA_AVALON_PIO_DATA(CON_VGA_IRQ_TX_BASE, 0x0);

        // >>> CRITICAL FIX 3: Phantom Key Wipe <<<
        // Wait for the VGA processor to consume the key press, then wipe it.
        // Otherwise, the next time you flip a switch, the VGA processor will
        // read a ghost button press and crash the switch handler logic!
        usleep(1500);
        IOWR_32DIRECT(SYSTEM_FLAGS_BASE, FLAG_CONTROL_KEY_PRESSED_MASK, 0x00);

        GPIO_state = GPIO_state | 0x1;
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_GPIO_BASE, GPIO_state);
        key_state = (key_state | CONTROL_KEY0_MASK) & CONTROL_KEY_MASK;
    }
}

void handle_key2(void) {
    if ((~key_state) & CONTROL_KEY1_MASK) {
        // 1. Tell the VGA processor exactly WHICH key was pressed
        IOWR_32DIRECT(SYSTEM_FLAGS_BASE, FLAG_CONTROL_KEY_PRESSED_MASK, CONTROL_KEY1_MASK);
        IOWR_32DIRECT(SYSTEM_FLAGS_BASE, FLAG_CONTROL_LAST_EVENT_TYPE, CONTROL_EVENT_KEY);

        // Force SDRAM write flush
        IORD_32DIRECT(SYSTEM_FLAGS_BASE, FLAG_CONTROL_LAST_EVENT_TYPE);

        // 2. Widen the IRQ Pulse
        IOWR_ALTERA_AVALON_PIO_DATA(CON_IMG_IRQ_TX_BASE, 0x1);
        IOWR_ALTERA_AVALON_PIO_DATA(CON_VGA_IRQ_TX_BASE, 0x1);

        usleep(100);

        IOWR_ALTERA_AVALON_PIO_DATA(CON_IMG_IRQ_TX_BASE, 0x0);
        IOWR_ALTERA_AVALON_PIO_DATA(CON_VGA_IRQ_TX_BASE, 0x0);

        // 3. Wipe Phantom Keys
        usleep(1500);
        IOWR_32DIRECT(SYSTEM_FLAGS_BASE, FLAG_CONTROL_KEY_PRESSED_MASK, 0x00);

        GPIO_state = GPIO_state | 0x2;
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_GPIO_BASE, GPIO_state);
        key_state = (key_state | CONTROL_KEY1_MASK) & CONTROL_KEY_MASK;
>>>>>>> Stashed changes
    }
}

void HEX_enable(void) {
    if (switch_state & 0x01) {
        HEX_enable_bit = 1;
    } else {
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, 0xFF);
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX1_BASE, 0xFF);
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX2_BASE, 0xFF);
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX3_BASE, 0xFF);
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX4_BASE, 0xFF);
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX5_BASE, 0xFF);
        HEX_enable_bit = 0;
        scroll_counter = 0;
        scroll_offset = 0;
    }
}

void handle_switch2(const char *message) {
    if ((switch_state & 0x02) && HEX_enable_bit) {
        char padded_message[64];
        int msg_len;
        if (message == 0) message = "";

        snprintf(padded_message, sizeof(padded_message), "    %s    ", message);
        msg_len = strlen(padded_message);
        scroll_counter++;

        if (scroll_counter >= scroll_speed) {
            char c5 = padded_message[scroll_offset];
            char c4 = padded_message[scroll_offset + 1];
            char c3 = padded_message[scroll_offset + 2];
            char c2 = padded_message[scroll_offset + 3];

            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX5_BASE, translator(c5));
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX4_BASE, translator(c4));
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX3_BASE, translator(c3));
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX2_BASE, translator(c2));

            scroll_offset++;
            if (scroll_offset > (msg_len - 4)) scroll_offset = 0;
            scroll_counter = 0;
        }
    } else {
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX5_BASE, 0xFF);
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX4_BASE, 0xFF);
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX3_BASE, 0xFF);
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX2_BASE, 0xFF);
        scroll_counter = 0;
        scroll_offset = 0;
    }
}

void handle_switch3(void) {
    if ((switch_state & 0x04) && HEX_enable_bit) {
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX1_BASE, translator('8'));
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, translator('7'));
    } else {
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX1_BASE, 0xFF);
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, 0xFF);
    }
}

<<<<<<< Updated upstream
/* ========================================================================
   MODIFIED: handle_switch4 now cycles through sound effects via Edge Detection
   ======================================================================== */
void handle_switch4(void)
{
    static int sw4_was_high = 0;
    static int sound_cycle_index = 0;

    // Check current hardware state of the switch (0x08 matching your mapping)
    int sw4_is_high = (switch_state & 0x08) ? 1 : 0;

    // Rising Edge Detected: Switch was just flipped UP
    if (sw4_is_high && !sw4_was_high) {
        printf("[Sound Test] Playing Effect #%d...\n", sound_cycle_index);

        switch (sound_cycle_index) {
            case 0:
                sfx_laser_shoot();
                break;
            case 1:
                sfx_explosion();
                break;
            case 2:
                sfx_portal();
                break;
            case 3:
                sfx_menu_blip();
                break;
            case 4:
                sfx_error_buzz();
                break;
            case 5:
            	sfx_eat_apple();
            	break;
            case 6:
            	sfx_end_screen();
            	break;
            default:
                play_speaker(0, 0);
                break;
        }

        // Advance to next sound effect slot (0 to 7)
        sound_cycle_index = (sound_cycle_index + 1) % 7;
    }

    // Fallback: If switch is left down, ensure speaker hardware is off
    if (!sw4_is_high) {
        play_speaker(0, 0);
    }

    // Latch history for the next iteration of main loop execution
    sw4_was_high = sw4_is_high;
}

int game_already_started = 0;

void handle_snake_game_switch(void) {
    if (switch_state & 0x10) {
        IOWR_32DIRECT(SHARED_FLAGS_BASE, FLAG_SYSTEM_POWER, 1);

        uint32_t current_game = IORD_32DIRECT(SHARED_FLAGS_BASE, FLAG_CURRENT_GAME);
        uint32_t game_running = IORD_32DIRECT(SHARED_FLAGS_BASE, FLAG_GAME_RUNNING);

        if (current_game == GAME_MODE_SNAKE && game_running == 0) {
            if (game_already_started == 0) {
                printf("\n[Control Core] Snake screen detected. Starting countdown...\n");

                int aborted = run_synchronized_countdown();

                if (!aborted) {
                    printf("[Control Core] Countdown complete! UNLOCKING SNAKE.\n");
                    IOWR_32DIRECT(SHARED_FLAGS_BASE, FLAG_GAME_RUNNING, 1);
                    game_already_started = 1;
                } else {
                    printf("[Control Core] Countdown aborted mid-way.\n");
                    game_already_started = 0;
                }
            }
        }
        else if (current_game != GAME_MODE_SNAKE) {
            game_already_started = 0;
        }
    }
    else {
        IOWR_32DIRECT(SHARED_FLAGS_BASE, FLAG_SYSTEM_POWER, 0);

        red_light(0);
        yellow_light(0);
        green_light(0);
        play_speaker(1000, 0);
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, 0xFF);

        game_already_started = 0;
    }
}

void handle_switch9(void) {
    int live_switches = IORD_ALTERA_AVALON_PIO_DATA(PIO_SW_BASE);
    IOWR_32DIRECT(SHARED_FLAGS_BASE, FLAG_CONTROL_SWITCH_STATE, live_switches);
=======
void handle_switch4(void) {
    if (switch_state & 0x08) {
        play_speaker(1000, 1);
    } else {
        play_speaker(1000, 0);
    }
>>>>>>> Stashed changes
}

void handle_snake_game_switch(void) {
    // Action 1: SW4 is flipped UP (Arcade Console Powered ON)
    if (switch_state & 0x10) {
        IOWR_32DIRECT(SYSTEM_FLAGS_BASE, FLAG_SYSTEM_POWER, 1);

        // Read current game state and running status from the dedicated system flags base
        uint32_t current_game = IORD_32DIRECT(SYSTEM_FLAGS_BASE, FLAG_CURRENT_GAME);
        uint32_t game_running = IORD_32DIRECT(SYSTEM_FLAGS_BASE, FLAG_GAME_RUNNING);

        if (current_game == GAME_MODE_SNAKE && game_running == 0) {
            if (game_already_started == 0) {
                printf("\n[Control Core] Snake screen detected. Starting countdown...\n");

                int aborted = run_synchronized_countdown();

                if (!aborted) {
                    printf("[Control Core] Countdown complete! UNLOCKING SNAKE.\n");
                    IOWR_32DIRECT(SYSTEM_FLAGS_BASE, FLAG_GAME_RUNNING, 1);
                    game_already_started = 1;
                } else {
                    printf("[Control Core] Countdown aborted mid-way.\n");
                    game_already_started = 0;
                }
            }
        } else if (current_game != GAME_MODE_SNAKE) {
            // Reset the countdown latch if user navigates away to another screen
            game_already_started = 0;
        }
    }
    // Action 2: SW4 is flipped DOWN (Arcade Console Powered OFF)
    else {
        IOWR_32DIRECT(SYSTEM_FLAGS_BASE, FLAG_SYSTEM_POWER, 0);

        // Instantly kill hardware peripherals for safety
        red_light(0);
        yellow_light(0);
        green_light(0);
        play_speaker(1000, 0);
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, 0xFF);

        game_already_started = 0;
    }
}

int translator(char a) {
    if (a >= '0' && a <= '9') {
        const int num_table[10] = {0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x90};
        return num_table[a - '0'];
    }
    if (a >= 'a' && a <= 'z') a = a - 'a' + 'A';
    if (a >= 'A' && a <= 'Z') {
        const int alpha_table[26] = {
            0x88, 0x83, 0xC6, 0xA1, 0x86, 0x8E, 0x90, 0x89, 0xF9, 0xF1,
            0x8A, 0xC7, 0xC8, 0xAB, 0xC0, 0x8C, 0x98, 0xAF, 0x92, 0x87,
            0xC1, 0xE3, 0x81, 0x89, 0x91, 0xA4
        };
        return alpha_table[a - 'A'];
    }
    return 0xFF;
}

void control_shared_flags_init(void)
{
    switch_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_SW_BASE) & 0x3FF;
    key_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_PB_BASE) & 0x3;

    if (switch_state & 0x10) {
        IOWR_32DIRECT(SHARED_FLAGS_BASE, FLAG_SYSTEM_POWER, 1);
    } else {
        IOWR_32DIRECT(SHARED_FLAGS_BASE, FLAG_SYSTEM_POWER, 0);
    }
}
