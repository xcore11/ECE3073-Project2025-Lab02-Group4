#include <stdint.h>

#ifndef CONTROL_H
#define CONTROL_H

/* shared SDRAM control flags */
#ifndef SHARED_FLAGS_BASE
#define CONTROL_START_FLAGS_BASE               0x06000000
#endif

#define FLAG_CONTROL_PROCESSOR_READY   0x10
#define FLAG_CONTROL_EVENT_SEQ         0x800
#define FLAG_CONTROL_KEY_STATE         0x804
#define FLAG_CONTROL_KEY_PRESSED_MASK  0x808
#define FLAG_CONTROL_SWITCH_STATE      0x80C
#define FLAG_CONTROL_SWITCH_EVENT_SEQ  0x810
#define FLAG_CONTROL_LAST_EVENT_TYPE   0x814
#define FLAG_CONTROL_LAST_EVENT_VALUE  0x818
#define FLAG_CONTROL_SPEAKER_OPTION	   0x81C
#define FLAG_CONTROL_LED_MODULE		   0x820
#define FLAG_CONTROL_LEDR			   0x824
#define FLAG_CONTROL_MESSAGE		   0x828 // Save 56 byte message (0x860 is next address)

#define CONTROL_EVENT_NONE             0
#define CONTROL_EVENT_KEY              1
#define CONTROL_EVENT_SWITCH           2

#define CONTROL_KEY0_MASK              0x00000001u
#define CONTROL_KEY1_MASK              0x00000002u
#define CONTROL_KEY_MASK               0x00000003u
#define CONTROL_SW_MASK                0x000003FFu

// for the extern semaphore
#include "includes.h"
///* =========================
//   Direct hardware base defines
//   ========================= */
//
//#define SPI_BASE                0x08011020
//#define PIO_SPEAKER_BASE        0x08011110
//#define PIO_LED_MODULE_BASE     0x08011120
//#define PIO_KEY_BASE            0x080111B0
//#define PIO_SW_BASE             0x080111C0
//#define PIO_LED_BASE            0x080111D0
//
//#define PIO_HEX5_BASE           0x08011140
//#define PIO_HEX4_BASE           0x08011150
//#define PIO_HEX3_BASE           0x08011160
//#define PIO_HEX2_BASE           0x08011170
//#define PIO_HEX1_BASE           0x08011180
//#define PIO_HEX0_BASE           0x080111b0
//
//#define PIO_SPI_SELECT_BASE     0x08011130
//#define PIO_IMGADDR_BASE 0x9050
//#define PIO_PIXELDATA_BASE 0x9070
//#define PIO_WREN_BASE 0x9040

// semaphore
extern OS_EVENT *input_update_sem;
extern OS_EVENT *leds_update_sem;

/* existing switch_key functions / HEX functions */
void control_shared_flags_init(void);
uint32_t control_get_switch_state(void);
uint32_t control_get_key_state(void);
void switch_setup(void);
void key_setup(void);
void img_rx_setup(void);
void vga_rx_setup(void);
void input_task(void* pdata);
void leds_update_task(void* pdata);
void HEX_task(void* pdata);
void handle_switch4(void);
int translator(char a);

/* speaker functions */
void play_speaker(int frequency, int on_off);

/* traffic led functions */
void red_light(int on_off);
void yellow_light(int on_off);
void green_light(int on_off);

/* accelerometer function */
int accel_init(void);
int accel_read_x(int *x);
int accel_read_y(int *y);
int accel_read_z(int *z);

#endif
