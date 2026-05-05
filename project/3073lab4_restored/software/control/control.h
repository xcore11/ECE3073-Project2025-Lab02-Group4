#include <stdint.h>

#ifndef SWITCHES_H
#define SWITCHES_H

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

/* existing switch / HEX functions */
void switch_setup(void);
void key_setup(void);
void img_rx_setup(void);
void vga_rx_setup(void);
void HEX_enable(int state);
void handle_switch2(int state, const char *message);
void handle_switch3(int state);
void handle_switch4(int state);
int translator(char a);

/* merged from speaker.h */
void play_speaker(int frequency, int on_off);

/* merged from traffic_led.h */
void red_light(int on_off);
void yellow_light(int on_off);
void green_light(int on_off);

/* accelerometer */
int accel_init(void);
int accel_read_x(int *x);
int accel_read_y(int *y);
int accel_read_z(int *z);

#endif
