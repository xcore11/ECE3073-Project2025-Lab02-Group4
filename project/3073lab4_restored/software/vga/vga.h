#ifndef VGA_H
#define VGA_H

#include <stdint.h>
#include "alt_types.h"

#define FRAMEBUFFER0_BASE 0x05000000
#define FRAMEBUFFER1_BASE 0x05100000
#define TEXT_BUFFER_BASE  0x05200000
#define STATUS_BASE       0x05209000

#define VGA_WIDTH   320
#define VGA_HEIGHT  240

int accel_init(void);
int accel_read_x(alt_32 *x);
int accel_read_y(alt_32 *y);
int accel_read_z(alt_32 *z);

void vga_init(void);
void vga_fill_background(unsigned char bg_color);
void vga_print_software_text(int pixel_x, int pixel_y, const char* text, unsigned char color);
void vga_draw_rectangle(int start_x, int start_y, int width, int height, unsigned char color);
void vga_draw_circle(int center_x, int center_y, int radius, unsigned char color);
void vga_draw_image(const unsigned char *image_array);
void vga_print_text(int char_x, int char_y, const char* text);
void process_dynamic_image(const unsigned char *raw_frame);

#endif
