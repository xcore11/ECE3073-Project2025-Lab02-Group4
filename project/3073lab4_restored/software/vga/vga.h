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

/*
   Native VGA color standard for this project: direct RGB332.

   One pixel is stored as one byte:
       bits 7:5 = red   intensity, 0 to 7
       bits 4:2 = green intensity, 0 to 7
       bits 1:0 = blue  intensity, 0 to 3

   The VGA controller expands this byte to the physical RGB444 pins:
       VGA_R[3:0] = expanded R
       VGA_G[3:0] = expanded G
       VGA_B[3:0] = expanded B

   There is no old 4-bit palette conversion.
*/
typedef uint8_t vga_color_t;

#define VGA_RGB332_MASK 0xFFu

/* Compile-time helper for custom RGB332 values. For constants, this becomes a constant. */
#define VGA_RGB332(r, g, b) \
    ((vga_color_t)(((((uint8_t)(r)) & 0x07u) << 5) | \
                   ((((uint8_t)(g)) & 0x07u) << 2) | \
                   (((uint8_t)(b)) & 0x03u)))
/* Direct literal RGB332 values. */
#define VGA_RGB332_BLACK   ((vga_color_t)0x00u)
#define VGA_RGB332_DARK    ((vga_color_t)0x49u)  /* dark gray-ish: R=2,G=2,B=1 */
#define VGA_RGB332_BLUE    ((vga_color_t)0x03u)
#define VGA_RGB332_GREEN   ((vga_color_t)0x1Cu)
#define VGA_RGB332_CYAN    ((vga_color_t)0x1Fu)
#define VGA_RGB332_RED     ((vga_color_t)0xE0u)
#define VGA_RGB332_PURPLE  ((vga_color_t)0xE3u)
#define VGA_RGB332_YELLOW  ((vga_color_t)0xFCu)
#define VGA_RGB332_WHITE   ((vga_color_t)0xFFu)

/* Project-wide color aliases used by menu/snake/draw/ship code. */
#define COL_BLACK   VGA_RGB332_BLACK
#define COL_DARK    VGA_RGB332_DARK
#define COL_BLUE    VGA_RGB332_BLUE
#define COL_GREEN   VGA_RGB332_GREEN
#define COL_CYAN    VGA_RGB332_CYAN
#define COL_RED     VGA_RGB332_RED
#define COL_PURPLE  VGA_RGB332_PURPLE
#define COL_YELLOW  VGA_RGB332_YELLOW
#define COL_WHITE   VGA_RGB332_WHITE

static inline vga_color_t vga_gray8(uint8_t gray8)
{
    uint8_t r = (uint8_t)(gray8 >> 5);  /* 3 bits */
    uint8_t g = (uint8_t)(gray8 >> 5);  /* 3 bits */
    uint8_t b = (uint8_t)(gray8 >> 6);  /* 2 bits */
    return VGA_RGB332(r, g, b);
}

int accel_init(void);
int accel_read_x(alt_32 *x);
int accel_read_y(alt_32 *y);
int accel_read_z(alt_32 *z);

void vga_init(void);
void vga_fill_background(vga_color_t bg_color);
void vga_print_software_text(int pixel_x, int pixel_y, const char* text, vga_color_t color);
void vga_draw_rectangle(int start_x, int start_y, int width, int height, vga_color_t color);
void vga_draw_circle(int center_x, int center_y, int radius, vga_color_t color);
void vga_draw_image(const vga_color_t *image_array);
void vga_print_text(int char_x, int char_y, const char* text);
void process_dynamic_image(const unsigned char *raw_frame);

#endif
