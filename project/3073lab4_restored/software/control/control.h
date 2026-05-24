#include <stdint.h>

#ifndef CONTROL_H
#define CONTROL_H

/*
   CONTROL processor shared memory map

   0x05212000 page:
     Existing live shared flags used by Control <-> IMG/VGA.
     Keep this as the dominant/current project base so KEY/SW events,
     VGA menu handling, snake/draw/battle and SFX flags do not break.

   0x06000000 page:
     New decoded DEBUG-control mailbox written by IMG/decoderdebug.c.
     The offsets below are reused, but with DEBUG_CONTROL_BASE.
*/
#ifndef SHARED_FLAGS_BASE
#define SHARED_FLAGS_BASE              0x05212000
#endif

#ifndef DEBUG_CONTROL_BASE
#define DEBUG_CONTROL_BASE             0x06000000
#endif

/* Normal Control -> IMG/VGA event flags at SHARED_FLAGS_BASE + offset. */
#define FLAG_CONTROL_PROCESSOR_READY   0x010
#define FLAG_CONTROL_EVENT_SEQ         0x800
#define FLAG_CONTROL_KEY_STATE         0x804
#define FLAG_CONTROL_KEY_PRESSED_MASK  0x808
#define FLAG_CONTROL_SWITCH_STATE      0x80C
#define FLAG_CONTROL_SWITCH_EVENT_SEQ  0x810
#define FLAG_CONTROL_LAST_EVENT_TYPE   0x814
#define FLAG_CONTROL_LAST_EVENT_VALUE  0x818

/* Decoded DEBUG-control mailbox at DEBUG_CONTROL_BASE + offset. */
#define FLAG_CONTROL_SPEAKER_OPTION    0x81C
#define FLAG_CONTROL_LED_MODULE        0x820
#define FLAG_CONTROL_LEDR              0x824
#define FLAG_CONTROL_MESSAGE           0x828
#define DEBUG_CONTROL_MESSAGE_BYTES    68

#define CONTROL_EVENT_NONE             0
#define CONTROL_EVENT_KEY              1
#define CONTROL_EVENT_SWITCH           2

#define CONTROL_KEY0_MASK              0x00000001u
#define CONTROL_KEY1_MASK              0x00000002u
#define CONTROL_KEY_MASK               0x00000003u

#define CONTROL_SW1_MASK               0x00000001u
#define CONTROL_SW2_MASK               0x00000002u
#define CONTROL_SW3_MASK               0x00000004u
#define CONTROL_SW4_MASK               0x00000008u
#define CONTROL_SW_MASK                0x000003FFu

/* Game/action SFX trigger flags written by VGA and consumed by Control. */
#define FLAG_SFX_EAT_APPLE             0x090
#define FLAG_SFX_GAME_OVER             0x094
#define FLAG_SFX_PORTAL                0x098
#define FLAG_SFX_BATTLE_HIT            0x09C
#define FLAG_SFX_BATTLE_MISS           0x0A0
#define FLAG_SFX_CHANGE_ARSENAL        0x0A4
#define FLAG_SFX_MENU_BLIP             0x0A8

/* Debug decoder command values written by IMG into DEBUG_CONTROL_BASE. */
#define DEBUG_CONTROL_CMD_NONE         0
#define DEBUG_CONTROL_CMD_BATCH        100

#define DEBUG_CONTROL_MASK_HEX_MESSAGE 0x00000001u
#define DEBUG_CONTROL_MASK_LED_MODULE  0x00000002u
#define DEBUG_CONTROL_MASK_LEDR        0x00000004u
#define DEBUG_CONTROL_MASK_SPEAKER     0x00000008u

#define DEBUG_OPTION_VALID             0x80000000u
#define DEBUG_OPTION_BLINK             0x40000000u
#define DEBUG_OPTION_SECONDS_SHIFT     16
#define DEBUG_OPTION_SECONDS_MASK      0x00FF0000u

#define DEBUG_LED_MODULE_RED           0x00000001u
#define DEBUG_LED_MODULE_YELLOW        0x00000002u
#define DEBUG_LED_MODULE_GREEN         0x00000004u

#define DEBUG_LEDR_FROM_LEFT           0x00000100u

#define DEBUG_SPEAKER_FREQ_MASK        0x0000FFFFu
#define DEBUG_SPEAKER_HAS_DURATION     0x40000000u

/* uC/OS-II objects shared by ISR/task code. */
#include "includes.h"
extern OS_EVENT *input_update_sem;
extern OS_EVENT *leds_update_sem;
extern volatile int control_sfx_busy;

/* RTOS tasks. */
void input_task(void *pdata);
void HEX_task(void *pdata);
void leds_update_task(void *pdata);
void speaker_switch_task(void *pdata);

/* Existing switch/key/IRQ setup and state helpers. */
void control_shared_flags_init(void);
uint32_t control_get_switch_state(void);
uint32_t control_get_key_state(void);
void switch_setup(void);
void key_setup(void);
void img_rx_setup(void);
void vga_rx_setup(void);

/* Legacy wrappers kept so older mains/files still compile. */
void HEX_enable(void);
void handle_key1(void);
void handle_key2(void);
void handle_switch2(const char *message);
void handle_switch3(void);
void handle_switch4(void);
int translator(char a);

/* Speaker driver. */
void play_speaker(int frequency, int on_off);

/* Traffic LED module driver and game SFX visual helpers. */
void red_light(int on_off);
void yellow_light(int on_off);
void green_light(int on_off);
void eat_apple(int trigger);
void game_over(int trigger);
void portal(int trigger);
void explosion(int trigger);
void miss(int trigger);

/* Debug-control executor. */
void debug_control_init(void);
void debug_control_update(void);
void debug_control_all_outputs_off(void);
void debug_control_copy_message(char *dst, int max_len);
void debug_control_service_speaker(int switch_enabled);

/* Accelerometer functions, kept for compatibility with existing project files. */
int accel_init(void);
int accel_read_x(int *x);
int accel_read_y(int *y);
int accel_read_z(int *z);

#endif
