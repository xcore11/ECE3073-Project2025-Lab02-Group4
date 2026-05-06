#include "io.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vga.h"
#include "system.h"
#include "altera_avalon_pio_regs.h"
#include <stdint.h>

/* =========================
   Accelerometer Configurations
   ========================= */
#define MOVEMENT_THRESHOLD 15
#define TILT_SCALE 5
#define TILT_THRESHOLD 300

/* =========================
   Screen Boundaries
   ========================= */
#define VGA_WIDTH 320
#define VGA_HEIGHT 240

/* =========================
   Shared SDRAM buffer space
   ========================= */

#define TEXT_BUFFER_BASE        0x05200000
#define TEXT_BUFFER_SIZE        256

#define STATUS_BASE             0x05201000

#define STATUS_TEXT_READY       0x00
#define STATUS_CMD_READY        0x04
#define STATUS_SPI_RX_COUNT     0x0C

#define STATUS_REG(offset)      (*(volatile uint32_t *)(STATUS_BASE + (offset)))

/* =========================
   Accelerometer direction (Extinct)
   =========================
#define CENTER 0
#define LEFT 1
#define RIGHT 2
#define UP 3
#define DOWN 4 */

/* =========================
   External Function from vga.c
   ========================= */
extern void vga_init(void);
extern void vga_fill_background(unsigned char bg_color);
extern void vga_draw_box(int center_x, int center_y, unsigned char color);

int main(void)
{
    volatile char *text_buffer = (volatile char *)TEXT_BUFFER_BASE;

    printf("VGA processor started\n");
    printf("Reading text buffer at 0x%08X\n", TEXT_BUFFER_BASE);
    fflush(stdout);

    // Accelerometer and direction settings
    int x, y, z;
    volatile int emergency_flag = 0;

    /* Version 1, 4 static direction
    int current_direction = CENTER;
    int previous_direction = -1; */

    // Initialize VGA
    vga_init();

	// Start cursor in the middle of the screen
	int cursor_x = VGA_WIDTH / 2;
	int cursor_y = VGA_HEIGHT / 2;

	// Track previous position
	int prev_x = cursor_x;
	int prev_y = cursor_y;

	// Failed Accel Initialization Check
	if (accel_init() != 0) {
	    printf("Accelerometer init failed\n");
	    while (1);
	}

	// Colour background
	vga_fill_background(0xFF);
	vga_draw_box(cursor_x, cursor_y, 0x00);

    while (1)
    {
    	// Read Status to initiate
        int status_x = accel_read_x(&x);
        int status_y = accel_read_y(&y);
        int status_z = accel_read_z(&z);

    	 // Conditions to display to VGA
    	if (status_x == 0 && status_y == 0){

    		// Emergency if overtilt
    		if (abs(x) > TILT_THRESHOLD || abs(y) > TILT_THRESHOLD){
    			if (emergency_flag == 0){
					emergency_flag = 1;
					printf("EMEGENCY STOP DETECTED\n");
					printf("Overtilt Detected! (X: %d, Y: %d)\n", x, y);
					// TODO: Placeholder for interrupt trigger here
    			}
    		}

			// Return to Normal operation protocols
			if (emergency_flag == 1){
				if (abs(x) <= TILT_THRESHOLD && abs(y) <= TILT_THRESHOLD){
					emergency_flag = 0;
					printf("Resuming Operation...\n");
				}
			}
			else{
				// Tilt back to resume operation
				if (abs(x) > MOVEMENT_THRESHOLD){
					cursor_x -= (x / TILT_SCALE);
				}
				if (abs(y) > MOVEMENT_THRESHOLD){
					cursor_y += (y / TILT_SCALE);
				}
			}


    		/* Version 1 Direction Implementation */
//            // Direction Initialization
//            if (x < -MOVEMENT_THRESHOLD){
//    		   current_direction = RIGHT;
//    		}
//            else if (x > MOVEMENT_THRESHOLD){
//    		   current_direction = LEFT;
//    		}
//            else if (y < -MOVEMENT_THRESHOLD){
//    		   current_direction = UP;
//    		}
//            else if (y > MOVEMENT_THRESHOLD) {
//    		   current_direction = DOWN;
//    		}
//            else {
//    		   current_direction = CENTER;
//    		}


    		// Screen boundary
    		if (cursor_x < 4) cursor_x = 4; // 4 prevents the 9x9 box from going off-screen
    		if (cursor_x > 315) cursor_x = 315;
    		if (cursor_y < 4) cursor_y = 4;
    		if (cursor_y > 235) cursor_y = 235;

    		// Draw the VGA
    		if (cursor_x != prev_x || cursor_y != prev_y) {

    		   // Erase the old box by painting over it with the background color (White)
    		   vga_draw_box(prev_x, prev_y, 0xFF);

    		   // Draw the new box at the new location (Black)
    		   vga_draw_box(cursor_x, cursor_y, 0x00);

    		   // Save the current location for the next loop
    		   prev_x = cursor_x;
    		   prev_y = cursor_y;

    		   printf("Dynamic Move -> X: %d, Y: %d\n", cursor_x, cursor_y);
    		   }
    	 }

    	/* Version 1 static direction*/
    	/* Test case for VGA implementation
    	 if (current_direction != previous_direction) {
    		vga_fill_background(0xFF);
    	    printf("Raw X: %4d, Y: %4d  --->  Direction: ", x, y);


//    	    // Print the human-readable word instead of just a number
//    	    switch(current_direction){
//    	        case CENTER: printf("CENTER\n"); vga_draw_box(160, 120, 0x00); break;
//    	        case LEFT:   printf("LEFT\n"); vga_draw_box(40, 120, 0x00); break;
//    	        case RIGHT:  printf("RIGHT\n"); vga_draw_box(280, 120, 0x00); break;
//    	        case UP:     printf("UP\n"); vga_draw_box(160, 40, 0x00); break;
//    	        case DOWN:   printf("DOWN\n"); vga_draw_box(160, 200, 0x00); break;
//    	       }
    	    previous_direction = current_direction;
    	 } */
    	 usleep(16000);
    }

    return 0;
}
