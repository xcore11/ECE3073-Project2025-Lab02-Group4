#ifndef CONTROL_H
#define CONTROL_H

#include <stdint.h>
#include <io.h>

/* Shared SDRAM control flags */
#ifndef SHARED_FLAGS_BASE
#define SHARED_FLAGS_BASE              0x05212000
#endif

#define FLAG_CONTROL_PROCESSOR_READY   0x10
#define FLAG_CONTROL_EVENT_SEQ         0x800
#define FLAG_CONTROL_KEY_STATE         0x804
#define FLAG_CONTROL_KEY_PRESSED_MASK  0x808
#define FLAG_CONTROL_SWITCH_STATE      0x80C
#define FLAG_CONTROL_SWITCH_EVENT_SEQ  0x810
#define FLAG_CONTROL_LAST_EVENT_TYPE   0x814
#define FLAG_CONTROL_LAST_EVENT_VALUE  0x818

// Snake Game
#define FLAG_EAT_APPLE                 0x090
#define FLAG_GAME_OVER                 0x094
#define FLAG_PORTAL                    0x900

// Ship Game
#define FLAG_EXPLOSION                 0x09C
#define FLAG_CHANGE_ARSENAL            0X0A0
#define FLAG_MISS                      0X0A4

// Menu Tilt Interaction
#define FLAG_ACCEL_MENU                0x098

#define CONTROL_EVENT_NONE             0
#define CONTROL_EVENT_KEY              1
#define CONTROL_EVENT_SWITCH           2

#define CONTROL_KEY0_MASK              0x00000001u
#define CONTROL_KEY1_MASK              0x00000002u
#define CONTROL_KEY_MASK               0x00000003u
#define CONTROL_SW_MASK                0x000003FFu

/* Core Setup & Handlers */
void control_shared_flags_init(void);
void switch_setup(void);
void key_setup(void);
void img_rx_setup(void);
void vga_rx_setup(void);

void HEX_enable(void);
void handle_key1(void);
void handle_key2(void);
void handle_switch2(const char *message);
void handle_switch3(void);
void handle_switch4(void);
void handle_snake_game_switch(void);
int translator(char a);

/* Peripherals */
void play_speaker(int frequency, int on_off);
void red_light(int on_off);
void yellow_light(int on_off);
void green_light(int on_off);
void eat_apple(int trigger);
void game_over(int trigger);

#endif /* CONTROL_H */
