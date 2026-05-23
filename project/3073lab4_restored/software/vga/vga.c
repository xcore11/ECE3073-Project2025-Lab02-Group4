#include "system.h"
#include "io.h"
#include "vga.h"

/* =========================
   VGA dimensions
   ========================= */

#define VGA_WIDTH   320
#define VGA_HEIGHT  240

/*
   IMPORTANT:
   Do not use TEXT_BUFFER_BASE here for ESP received text.
   TEXT_BUFFER_BASE is now used by the image/SPI processor SDRAM text storage.

   This VGA_CHAR_BUFFER_BASE is only for the old character-buffer placeholder
   function vga_print_text().
*/
#ifndef VGA_CHAR_BUFFER_BASE
#define VGA_CHAR_BUFFER_BASE 0x00000000
#endif

static int animation_offset = 0;

/* =========================
   Small delay for write pulse
   ========================= */

static void delay(int loops)
{
    volatile int i;

    for (i = 0; i < loops; i++)
    {
        /* leave empty */
    }
}

/* =========================
   Pixel drawing
   ========================= */

static void draw_pixel(int x, int y, vga_color_t color)
{
    if (x < 0 || x >= VGA_WIDTH || y < 0 || y >= VGA_HEIGHT)
    {
        return;
    }

    /*
       Convert 2D coordinate into framebuffer address.
    */
    int address = x + (y * VGA_WIDTH);

    /*
       Hardware pixel data is now native RGB332.
       Only the lower 8 bits are sent to the pixel RAM.
    */
    color = (vga_color_t)(color & VGA_RGB332_MASK);

    IOWR_32DIRECT(PIO_IMGADDR_BASE, 0, address);
    IOWR_32DIRECT(PIO_PIXELDATA_BASE, 0, (uint32_t)color);

    /*
       Write enable pulse.
    */
    IOWR_32DIRECT(PIO_WREN_BASE, 0, 1);
    delay(5);
    IOWR_32DIRECT(PIO_WREN_BASE, 0, 0);
}

void vga_init(void)
{
    IOWR_32DIRECT(PIO_WREN_BASE, 0, 0);
    animation_offset = 0;
}

void vga_fill_background(vga_color_t bg_color)
{
    int x;
    int y;

    for (y = 0; y < VGA_HEIGHT; y++)
    {
        for (x = 0; x < VGA_WIDTH; x++)
        {
            draw_pixel(x, y, bg_color);
        }
    }
}

/* ----------------------------------------------------
   SOFTWARE TEXT RENDERING DICTIONARIES
   ---------------------------------------------------- */

/* Dictionary for Letters A-Z */
static const unsigned char font_AZ[26][8] = {
    {0x18, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x00}, /* A */
    {0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00}, /* B */
    {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00}, /* C */
    {0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00}, /* D */
    {0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x7E, 0x00}, /* E */
    {0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x60, 0x00}, /* F */
    {0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3E, 0x00}, /* G */
    {0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00}, /* H */
    {0x3E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3E, 0x00}, /* I */
    {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x6C, 0x38, 0x00}, /* J */
    {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00}, /* K */
    {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00}, /* L */
    {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00}, /* M */
    {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00}, /* N */
    {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00}, /* O */
    {0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00}, /* P */
    {0x3C, 0x66, 0x66, 0x66, 0x66, 0x6C, 0x36, 0x00}, /* Q */
    {0x7C, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0x66, 0x00}, /* R */
    {0x3C, 0x60, 0x38, 0x0E, 0x06, 0x66, 0x3C, 0x00}, /* S */
    {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00}, /* T */
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00}, /* U */
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00}, /* V */
    {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}, /* W */
    {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00}, /* X */
    {0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00}, /* Y */
    {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00}  /* Z */
};

/* Dictionary for Numbers 0-9 */
static const unsigned char font_09[10][8] = {
    {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00}, /* 0 */
    {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00}, /* 1 */
    {0x3C, 0x66, 0x06, 0x0C, 0x30, 0x60, 0x7E, 0x00}, /* 2 */
    {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00}, /* 3 */
    {0x0C, 0x1C, 0x2C, 0x4C, 0x7E, 0x0C, 0x0C, 0x00}, /* 4 */
    {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00}, /* 5 */
    {0x3C, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x3C, 0x00}, /* 6 */
    {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00}, /* 7 */
    {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00}, /* 8 */
    {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x66, 0x3C, 0x00}  /* 9 */
};

/* =========================
   Software text drawing
   ========================= */

void vga_print_software_text(int pixel_x, int pixel_y, const char* text, vga_color_t color)
{
    int i = 0;

    while (text[i] != '\0')
    {
        char c = text[i];
        const unsigned char* letter_pixels = 0;

        /*
           ESP text is lowercase.
           Convert to uppercase so the existing font table can draw it.
        */
        if (c >= 'a' && c <= 'z')
        {
            c = c - 'a' + 'A';
        }

        if (c >= 'A' && c <= 'Z')
        {
            letter_pixels = font_AZ[c - 'A'];
        }
        else if (c >= '0' && c <= '9')
        {
            letter_pixels = font_09[c - '0'];
        }
        else
        {
            /*
               Space or unsupported character.
               Leave blank but still advance cursor.
            */
            letter_pixels = 0;
        }

        if (letter_pixels != 0)
        {
            int row;
            int col;

            for (row = 0; row < 8; row++)
            {
                unsigned char pixel_row = letter_pixels[row];

                for (col = 0; col < 8; col++)
                {
                    if ((pixel_row >> (7 - col)) & 1)
                    {
                        draw_pixel(pixel_x + (i * 8) + col,
                                   pixel_y + row,
                                   color);
                    }
                }
            }
        }

        i++;
    }
}

/* =========================
   Shapes
   ========================= */

void vga_draw_rectangle(int start_x, int start_y, int width, int height, vga_color_t color)
{
    int x;
    int y;

    for (y = start_y; y < start_y + height; y++)
    {
        for (x = start_x; x < start_x + width; x++)
        {
            draw_pixel(x, y, color);
        }
    }
}

void vga_draw_circle(int center_x, int center_y, int radius, vga_color_t color)
{
    int x;
    int y;

    for (y = -radius; y <= radius; y++)
    {
        for (x = -radius; x <= radius; x++)
        {
            if ((x * x) + (y * y) <= (radius * radius))
            {
                draw_pixel(center_x + x, center_y + y, color);
            }
        }
    }
}

/* =========================
   Full screen image draw
   Used only if you have a 320x240 RGB332 image array.
   ========================= */

void vga_draw_image(const vga_color_t *image_array)
{
    int x;
    int y;

    for (y = 0; y < VGA_HEIGHT; y++)
    {
        for (x = 0; x < VGA_WIDTH; x++)
        {
            int array_index = x + (y * VGA_WIDTH);
            draw_pixel(x, y, image_array[array_index]);
        }
    }
}

/* =========================
   Character-buffer placeholder
   Not used for ESP SDRAM text display.
   ========================= */

void vga_print_text(int char_x, int char_y, const char* text)
{
    int i = 0;

    while (text[i] != '\0')
    {
        int offset = (char_y * 40) + char_x + i;

        /*
           This is only for a real VGA character buffer IP.
           It should NOT write to ESP TEXT_BUFFER_BASE.
        */
        IOWR_8DIRECT(VGA_CHAR_BUFFER_BASE, offset, text[i]);

        i++;
    }
}

/* =========================
   Dynamic image processing placeholder
   ========================= */

void process_dynamic_image(const unsigned char *raw_frame)
{
    int x;
    int y;
    vga_color_t processed_pixel;

    for (y = 24; y < (24 + 92); y++)
    {
        for (x = 114; x < (114 + 92); x++)
        {
            unsigned char pixel = raw_frame[(x - 114) + ((y - 24) * 92)];

            processed_pixel = vga_gray8(pixel);

            draw_pixel(x, y, processed_pixel);
        }
    }
}
