#include <stdint.h>

#ifndef VGA_H
#define VGA_H

/* accelerometer */
int accel_init(void);
int accel_read_x(int *x);
int accel_read_y(int *y);
int accel_read_z(int *z);

/* VGA */
void vga_init(void);
void vga_step(void);

/* shared SDRAM */
#define FRAMEBUFFER0_BASE 0x05000000
#define FRAMEBUFFER1_BASE 0x05100000
#define TEXT_BUFFER_BASE  0x05200000
#define STATUS_BASE       0x05201000

#endif
