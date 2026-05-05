#include "system.h"
#include "io.h"
#include "vga.h"

// Define the desired height and width
#define VGA_WIDTH 320
#define VGA_HEIGHT 240

// TODO: SDRAM PLACEHOLDER:
#ifndef TEXT_BUFFER_BASE
#define TEXT_BUFFER_BASE 0x00000000
#endif

// TODO: #include "image.h"
// Expected an image converted into an array of hex values
// The Pixel Array: A large constant array representing the image data. For your centered box, it would likely be const unsigned char my_image[8464]; (92x92 pixels).
//Dimensions: Definitions for the image width and height to avoid "magic numbers" in your functions.
// Color Palette: If you aren't using 8-bit true color, it might contain a lookup table for your specific VGA color depth.

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

// ----------------------------------------------------
// SOFTWARE TEXT RENDERING DICTIONARIES
// ----------------------------------------------------
// Dictionary for Letters A-Z
static const unsigned char font_AZ[26][8] = {
    {0x18, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x00}, // A
    {0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00}, // B
    {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00}, // C
    {0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00}, // D
    {0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x7E, 0x00}, // E
    {0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x60, 0x00}, // F
    {0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3E, 0x00}, // G
    {0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00}, // H
    {0x3E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3E, 0x00}, // I
    {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x6C, 0x38, 0x00}, // J
    {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00}, // K
    {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00}, // L
    {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00}, // M
    {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00}, // N
    {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00}, // O
    {0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00}, // P
    {0x3C, 0x66, 0x66, 0x66, 0x66, 0x6C, 0x36, 0x00}, // Q
    {0x7C, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0x66, 0x00}, // R
    {0x3C, 0x60, 0x38, 0x0E, 0x06, 0x66, 0x3C, 0x00}, // S
    {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00}, // T
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00}, // U
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00}, // V
    {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}, // W
    {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00}, // X
    {0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00}, // Y
    {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00}  // Z
};

// Dictionary for Numbers 0-9
static const unsigned char font_09[10][8] = {
    {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00}, // 0
    {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00}, // 1
    {0x3C, 0x66, 0x06, 0x0C, 0x30, 0x60, 0x7E, 0x00}, // 2
    {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00}, // 3
    {0x0C, 0x1C, 0x2C, 0x4C, 0x7E, 0x0C, 0x0C, 0x00}, // 4
    {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00}, // 5
    {0x3C, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x3C, 0x00}, // 6
    {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00}, // 7
    {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00}, // 8
    {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x66, 0x3C, 0x00}  // 9
};

// Function to manually paint letters and numbers using PIO
void vga_print_software_text(int pixel_x, int pixel_y, const char* text, unsigned char color) {
    int i = 0;
    while (text[i] != '\0') {
        char c = text[i];
        const unsigned char* letter_pixels = 0;

        // Check if it's a letter A-Z
        if (c >= 'A' && c <= 'Z') {
            letter_pixels = font_AZ[c - 'A'];
        }
        // Check if it's a number 0-9
        else if (c >= '0' && c <= '9') {
            letter_pixels = font_09[c - '0'];
        }
        // Spaces are skipped (handled by the i++ increment moving us over 8 pixels)

        // If we found a valid character in our dictionaries, paint it
        if (letter_pixels != 0) {
            // Loop through the 8 rows of the character
            for (int row = 0; row < 8; row++) {
                unsigned char pixel_row = letter_pixels[row];

                // Loop through the 8 pixels in this row
                for (int col = 0; col < 8; col++) {
                    // Check if the binary bit is a 1
                    if ((pixel_row >> (7 - col)) & 1) {
                        // Paint the pixel!
                        draw_pixel(pixel_x + (i * 8) + col, pixel_y + row, color);
                    }
                }
            }
        }
        i++; // Move 8 pixels to the right for the next character
    }
}

// Draws a filled rectangle of any size
void vga_draw_rectangle(int start_x, int start_y, int width, int height, unsigned char color) {
    int x, y;
    for (y = start_y; y < start_y + height; y++) {
        for (x = start_x; x < start_x + width; x++) {
            draw_pixel(x, y, color);
        }
    }
}

void vga_draw_circle(int center_x, int center_y, int radius, unsigned char color) {
    int x, y;
    for (y = -radius; y <= radius; y++) {
        for (x = -radius; x <= radius; x++) {
            // If the pixel is within the radius distance from the center, draw it!
            if ((x * x) + (y * y) <= (radius * radius)) {
                draw_pixel(center_x + x, center_y + y, color);
            }
        }
    }
}

// =======================================================================
// FUTURE IMPLEMENTATION PLACEHOLDERS
// TODO: SDRAM / Image Arrays changed here for image, text only changes address
// =======================================================================

// 1. Function to draw a 320x240 image array using your PIO ports
void vga_draw_image(const unsigned char *image_array) {
    int x, y;
    for (y = 0; y < VGA_HEIGHT; y++) {
        for (x = 0; x < VGA_WIDTH; x++) {
            // Calculate 1D array index from 2D coordinates
            int array_index = x + (y * VGA_WIDTH);

            // Paint it to the monitor
            draw_pixel(x, y, image_array[array_index]);
        }
    }
}

// 2. Function to write text directly to your Character Buffer SDRAM IP
void vga_print_text(int char_x, int char_y, const char* text) {
    int i = 0;
    while (text[i] != '\0') {
        int offset = (char_y * 40) + char_x + i;
        // Bypasses the CPU cache
        IOWR_8DIRECT(TEXT_BUFFER_BASE, offset, text[i]);
        i++;
    }
}

// =======================================================================
// FUTURE PLACEHOLDER: Dynamic Image Processing Loop
// TODO: Process Dynamic images
// =======================================================================


void process_dynamic_image(const unsigned char *raw_frame) {
    int x, y;
    unsigned char processed_pixel;

    for (y = 24; y < (24 + 92); y++) {      // Only process the 92x92 center
        for (x = 114; x < (114 + 92); x++) {

            // 1. Fetch pixel from the source (placeholder for camera data)
            unsigned char pixel = raw_frame[(x-114) + ((y-24) * 92)];

            // 2. IMAGE PROCESSING PLACEHOLDER
            // Example: Simple Grayscale or Thresholding
            processed_pixel = (pixel > 0x80) ? 0xFF : 0x00;

            // 3. Update the display in real-time
            draw_pixel(x, y, processed_pixel);
        }
    }
}
