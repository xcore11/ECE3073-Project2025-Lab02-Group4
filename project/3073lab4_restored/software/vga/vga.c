#include "system.h"
#include "io.h"
#include "vga.h"

// Define the desired height and width
#define VGA_WIDTH 320
#define VGA_HEIGHT 240

static int animation_offset = 0;


// Delay
static void delay(int loops) {
    volatile int i;
    for (i = 0; i < loops; i++) {
        // Leave it be
    }
}

// Paint pixels on the monitor
static void draw_pixel(int x, int y, unsigned char color) {
	// Returns null if said accelerometer detected is exceeding set width and height
	if (x < 0 || x >= VGA_WIDTH || y < 0 || y >= VGA_HEIGHT) return;

    // Converts 2D grid to 1D to be read by memory
    int address = x + (y * VGA_WIDTH);

    // To set address and color (4 bits)
    IOWR_32DIRECT(PIO_IMGADDR_BASE, 0, address);
    IOWR_32DIRECT(PIO_PIXELDATA_BASE, 0, color);

    // Sends pulse to write enable to create a clock
    IOWR_32DIRECT(PIO_WREN_BASE, 0, 1);
    delay(5);
    IOWR_32DIRECT(PIO_WREN_BASE, 0, 0);
}

void vga_init(void) {
    IOWR_32DIRECT(PIO_WREN_BASE, 0, 0);
    animation_offset = 0;
}

void vga_fill_background(unsigned char bg_color) {
    int x, y;
    for (y = 0; y < VGA_HEIGHT; y++) {
        for (x = 0; x < VGA_WIDTH; x++) {
            draw_pixel(x, y, bg_color);
        }
    }
}

// Draws a nice big 9x9 box at the exact coordinates you tell it
void vga_draw_box(int center_x, int center_y, unsigned char color) {
    int i, j;
    for(i = -4; i <= 4; i++) {
        for(j = -4; j <= 4; j++) {
            draw_pixel(center_x + i, center_y + j, color);
        }
    }
}
