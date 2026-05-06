#include "io.h"
#include <unistd.h>
#include <stdio.h>
#include "vga.h"
#include "system.h"
// #include "image.h"

//// TODO: SDRAM BASE AND OFFSET
//#define SDRAM_BASE          0x08000000  // This matches your Qsys address
//#define SENSOR_DATA_OFFSET  0x00001000  // Example offset where hardware writes data

extern void vga_init(void);

int main(void)
{
	// TODO: Incoming data, insert offset and base
	char sensor_val_str[10];
	// volatile int* sdram_sensor_ptr = (volatile int*)(SDRAM_BASE + SENSOR_DATA_OFFSET);

    // Initialize the VGA Hardware
    vga_init();

    printf("Drawing Backgrounds...\n");
    // We will paint to segregate regions
    // Image
    // First 2 are starting coords, 3rd and 4th are width and height
    vga_draw_rectangle(0, 0, 320, 140, 0x03);
    // Text
    vga_draw_rectangle(0, 140, 320, 100, 0x00);

    // Segregate image and text box
    // Math: width (320 -92) / 2 = 114
    // Math: height (140 - 92) / 2 = 24
    printf("Drawing Image Box...\n");
    vga_draw_rectangle(114, 24, 92, 92, 0xE0);

    printf("Drawing Text Box...\n");
    // We offset the text slightly from the edge of the box (+4 pixels)
    // so it doesn't touch the exact border.
    // Each letter is 8 pixels tall. If we drop down by 20 pixels,
    // it leaves line space
    // Ignore the 2nd row im acoustic
    vga_print_software_text(10, 150, "SYSTEM OK", 0xFF);
    vga_print_software_text(10, 170, "67676767676767", 0xFF);
    vga_print_software_text(10, 190, "CENTERED!", 0xFF);

    while(1) {
    	// TODO: change the mock sensor data to what comes out from sdram
    	// Fetch data values in SDRAM
//    	int current_val = *sdram_sensor_ptr;
//    	sprintf(sensor_val_str, "VAL %02d", current_val);

    	// 2. Clear ONLY the small area where the number is (to avoid flickering)
        vga_draw_rectangle(118, 190, 80, 10, 0x00);

    	// 3. Update the text dynamically
    	vga_print_software_text(118, 190, sensor_val_str, 0xFF);

    	// Small delay to make it readable
    	usleep(100000);
    }

    return 0;
}
