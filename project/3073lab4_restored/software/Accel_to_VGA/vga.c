#include "system.h"
#include "io.h"

#define VGA_WIDTH 320
#define VGA_HEIGHT 240

// Keep track of where the pixel WAS, so we can erase it later
static int prev_x = VGA_WIDTH / 2;  // Start in the middle
static int prev_y = VGA_HEIGHT / 2;

// Delay
static void delay(int loops) {
    volatile int i;
    for (i = 0; i < loops; i++) {}
}

// Paint pixels on the monitor
static void draw_pixel(int x, int y, unsigned char color) {
    // 1. Boundary check! Don't write outside the screen, or your Nios II might crash
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

    // Optional: You could write a loop here to wipe the screen to black ONCE at boot
}

// Use this function in main.c!
void vga_draw_cursor(int target_x, int target_y, unsigned char color) {
    // 1. Erase the old pixel (Assuming color 0x00 is black)
    draw_pixel(prev_x, prev_y, 0x00);

    // 2. Draw the new pixel at the new accelerometer location
    draw_pixel(target_x, target_y, color);

    // 3. Save this location so we can erase it on the next loop
    prev_x = target_x;
    prev_y = target_y;
}
