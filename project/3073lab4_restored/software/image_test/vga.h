#include <stdint.h>
#include "alt_types.h"

#ifndef VGA_H
#define VGA_H

/* accelerometer */
int accel_init(void);
int accel_read_x(alt_32 *x);
int accel_read_y(alt_32 *y);
int accel_read_z(alt_32 *z);

/* VGA */
void vga_init(void);
void vga_step(void);
void vga_draw_image(const unsigned char *image_array);
void vga_draw_circle(int center_x, int center_y, int radius, unsigned char color);
void vga_draw_rectangle(int start_x, int start_y, int width, int height, unsigned char color);
void vga_print_software_text(int pixel_x, int pixel_y, const char* text, unsigned char color);

/* shared SDRAM */
#define FRAMEBUFFER0_BASE 0x05000000
#define FRAMEBUFFER1_BASE 0x05100000
#define TEXT_BUFFER_BASE  0x05200000
#define STATUS_BASE       0x05201000

#endif
