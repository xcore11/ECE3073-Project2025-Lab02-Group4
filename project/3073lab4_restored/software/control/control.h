#ifndef CONTROL_H
#define CONTROL_H

#include <stdint.h>
#include "includes.h"

/* ============================================================
   Shared SDRAM map
   ============================================================ */

/* Normal game/menu/key/switch flags read by IMG/VGA. */
#ifndef SHARED_FLAGS_BASE
#define SHARED_FLAGS_BASE               0x05212000
#endif

/* Debug/peripheral mailbox requested by user: effective 0x06000000 + offset. */
#ifndef DEBUG_CONTROL_BASE
#define DEBUG_CONTROL_BASE              0x06000000
#endif

/* Existing system/game flags at SHARED_FLAGS_BASE. */
#define FLAG_SYSTEM_MAGIC               0x000
#define FLAG_SESSION_STARTED            0x004
#define FLAG_IMAGE_PROCESSOR_READY      0x008
#define FLAG_VGA_PROCESSOR_READY        0x00C
#define FLAG_CONTROL_PROCESSOR_READY    0x010
#define FLAG_CURRENT_MENU               0x014
#define FLAG_CURRENT_GAME               0x018
#define FLAG_GAME_RUNNING               0x01C
#define FLAG_DEBUG_MODE                 0x020
#define FLAG_IMAGE_READY                0x024
#define FLAG_TEXT_READY_SHARED          0x028
#define FLAG_VGA_DISPLAY_DONE           0x02C
#define FLAG_LAST_COMMAND               0x030
#define FLAG_LAST_ERROR_SHARED          0x034
#define FLAG_MENU_ENTER_EVENT           0x038
#define FLAG_MENU_EXIT_EVENT            0x03C
#define FLAG_PANEL_MODE_SEQ             0x08C

/* Control -> IMG/VGA input flags at SHARED_FLAGS_BASE. */
#define FLAG_CONTROL_EVENT_SEQ          0x800
#define FLAG_CONTROL_KEY_STATE          0x804
#define FLAG_CONTROL_KEY_PRESSED_MASK   0x808
#define FLAG_CONTROL_SWITCH_STATE       0x80C
#define FLAG_CONTROL_SWITCH_EVENT_SEQ   0x810
#define FLAG_CONTROL_LAST_EVENT_TYPE    0x814
#define FLAG_CONTROL_LAST_EVENT_VALUE   0x818

/* Debug/peripheral command mailbox at DEBUG_CONTROL_BASE. */
#define FLAG_CONTROL_SPEAKER_OPTION     0x81C
#define FLAG_CONTROL_LED_MODULE         0x820
#define FLAG_CONTROL_LEDR               0x824
#define FLAG_CONTROL_MESSAGE            0x828
#define DEBUG_CONTROL_MESSAGE_BYTES     68

#define DEBUG_CONTROL_CMD_NONE          0
#define DEBUG_CONTROL_CMD_BATCH         100

#define DEBUG_CONTROL_MASK_HEX_MESSAGE  0x00000001u
#define DEBUG_CONTROL_MASK_LED_MODULE   0x00000002u
#define DEBUG_CONTROL_MASK_LEDR         0x00000004u
#define DEBUG_CONTROL_MASK_SPEAKER      0x00000008u

#define DEBUG_OPTION_VALID              0x80000000u
#define DEBUG_OPTION_BLINK              0x40000000u
#define DEBUG_OPTION_SECONDS_SHIFT      16
#define DEBUG_OPTION_SECONDS_MASK       0x00FF0000u

#define DEBUG_LED_MODULE_RED            0x00000001u
#define DEBUG_LED_MODULE_YELLOW         0x00000002u
#define DEBUG_LED_MODULE_GREEN          0x00000004u

#define DEBUG_LEDR_FROM_LEFT            0x00000100u
#define DEBUG_SPEAKER_FREQ_MASK         0x0000FFFFu
#define DEBUG_SPEAKER_HAS_DURATION      0x40000000u

/* Realtime activity flags. */
#define FLAG_RT_ACTIVITY_SEQ            0x870
#define FLAG_RT_PANEL_MODE              0x874

/* Snake status flags already produced by snake.c. */
#define SNAKE_FLAG_GAME_STATE           0x044
#define SNAKE_FLAG_SCORE                0x048
#define SNAKE_FLAG_EVENT_FLAGS          0x054
#define SNAKE_FLAG_APPLE_COUNT          0x088
#define SNAKE_STATE_RUNNING             1
#define SNAKE_STATE_LOST                2

/* One-shot SFX flags at SHARED_FLAGS_BASE. Safe region after battle grid mirror. */
#define FLAG_SFX_EAT_APPLE              0xC40
#define FLAG_SFX_GAME_OVER              0xC44
#define FLAG_SFX_PORTAL                 0xC48
#define FLAG_SFX_BATTLE_HIT             0xC4C
#define FLAG_SFX_BATTLE_MISS            0xC50
#define FLAG_SFX_CHANGE_ARSENAL         0xC54
#define FLAG_SFX_MENU_BLIP              0xC58
#define FLAG_SFX_CLICK                  0xC5C
#define FLAG_SFX_ENTER_SNAKE            0xC60
#define FLAG_SFX_ENTER_DRAW             0xC64
#define FLAG_SFX_ENTER_BATTLE           0xC68
#define FLAG_SFX_CLEAR                  0xC6C
#define FLAG_SFX_SNAKE_TURN             0xC70

/* Battle status exported by ship.c for Control peripherals. */
#define FLAG_BATTLE_LOADED_SHIP_CELLS   0xC80
#define FLAG_BATTLE_DESTROYED_CELLS     0xC84
#define FLAG_BATTLE_WIN                 0xC88
#define FLAG_BATTLE_CROSS_LEFT          0xC8C
#define FLAG_BATTLE_SQUARE_LEFT         0xC90
#define FLAG_BATTLE_SELECTED_BOMB       0xC94

/* Draw status exported by draw.c for Control peripherals. */
#define FLAG_DRAW_RED_COUNT             0xCC0
#define FLAG_DRAW_GREEN_COUNT           0xCC4
#define FLAG_DRAW_BLUE_COUNT            0xCC8
#define FLAG_DRAW_YELLOW_COUNT          0xCCC
#define FLAG_DRAW_BLACK_COUNT           0xCD0
#define FLAG_DRAW_WHITE_COUNT           0xCD4
#define FLAG_DRAW_OTHER_COUNT           0xCD8

/* Game identifiers used by VGA main.c. */
#define GAME_MODE_MENU                  0
#define GAME_MODE_SNAKE                 1
#define GAME_MODE_DRAW                  2
#define GAME_MODE_DEBUG                 3
#define GAME_MODE_BATTLE                4

#define MENU_SELECT_BATTLE              0
#define MENU_SELECT_SNAKE               1
#define MENU_SELECT_DRAW                2
#define MENU_SELECT_DEBUG               3
#define MENU_SELECT_NONE                0xFFFFFFFFu

/* Control event definitions. */
#define CONTROL_EVENT_NONE              0
#define CONTROL_EVENT_KEY               1
#define CONTROL_EVENT_SWITCH            2

#define CONTROL_KEY0_MASK               0x00000001u
#define CONTROL_KEY1_MASK               0x00000002u
#define CONTROL_KEY_MASK                0x00000003u
#define CONTROL_SW_MASK                 0x000003FFu

/* User-requested master switches. */
#define CONTROL_SW1_HEX_MASTER          0x00000001u
#define CONTROL_SW2_SCROLL_MASTER       0x00000002u
#define CONTROL_SW3_CPU_MASTER          0x00000004u
#define CONTROL_SW4_SPEAKER_MASTER      0x00000008u

/* LED module bits used by traffc_led.c. */
#define LED_MODULE_RED_BIT              0x1u
#define LED_MODULE_YELLOW_BIT           0x2u
#define LED_MODULE_GREEN_BIT            0x4u

/* ============================================================
   RTOS semaphores
   ============================================================ */
extern OS_EVENT *input_update_sem;
extern OS_EVENT *leds_update_sem;

/* ============================================================
   Control functions
   ============================================================ */
void control_shared_flags_init(void);
uint32_t control_get_switch_state(void);
uint32_t control_get_key_state(void);

void switch_setup(void);
void key_setup(void);
void img_rx_setup(void);
void vga_rx_setup(void);

void input_task(void *pdata);
void HEX_task(void *pdata);
void leds_update_task(void *pdata);
void sfx_task(void *pdata);

void handle_switch4(void);
int translator(char a);

/* speaker functions */
void play_speaker(int frequency, int on_off);

/* traffic LED module functions */
void red_light(int on_off);
void yellow_light(int on_off);
void green_light(int on_off);

/* debug-control executor */
void debug_control_init(void);
void debug_control_update(void);
void debug_control_all_outputs_off(void);
void debug_control_copy_message(char *dst, int max_len);
void debug_control_service_speaker(int switch_enabled);

/* sound effects */
void sound_init(void);
void sfx_laser_shoot(void);
void sfx_explosion(void);
void sfx_portal(void);
void sfx_menu_blip(void);
void sfx_error_buzz(void);
void sfx_eat_apple(void);
void sfx_end_screen(void);
void sfx_change(void);
void sfx_click(void);
void sfx_enter_snake(void);
void sfx_enter_draw(void);
void sfx_enter_battle(void);
void sfx_clear(void);
void sfx_snake_turn(void);

#endif /* CONTROL_H */
