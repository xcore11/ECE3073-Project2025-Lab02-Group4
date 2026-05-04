#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "system.h"
#include "io.h"

// The "Deadzone" Threshold: Ignore tiny sensor noise under this value
#define MOVEMENT_THRESHOLD 15
#define ACCEL_THRESHOLD 5000

// External functions from accel.c
extern int accel_init();
extern int accel_read_x(int *x);
extern int accel_read_y(int *y);
extern int accel_read_z(int *z);

// External functions from vga.c
extern void vga_init(void);
extern void vga_draw_cursor(int target_x, int target_y, unsigned char color);

int x_raw, y_raw, z_raw;

int main() {
    printf("System Starting: Initializing VGA and Accelerometer...\n");

    // 1. Initialize VGA
    vga_init();

    // 2. Initialize Accelerometer
    if (accel_init() != 0) {
        printf("Error: Accelerometer could not be opened.\n");
    } else {
        printf("Hardware initialized successfully! Waiting for movement...\n");
    }
     int accel_counter = ACCEL_THRESHOLD;
     int x, y, z;

    // 3. Main Application Loop
    while(1) {
    	accel_counter++;

   	    if (accel_counter >= 5000)
   	    {
   	            accel_counter = 0;

   	            accel_read_x(&x);
   	            accel_read_y(&y);
   	            accel_read_z(&z);

   	            printf("Accel X = %d, Y = %d, Z = %d\n", x, y, z);
   	     }

        // Check if reads were successful
//        if (status_x == 0 && status_y == 0) {
//
//            // --- THRESHOLD LOGIC ---
//            // Only update the screen if the board is tilted past the noise level
//            if (abs(x_raw) > MOVEMENT_THRESHOLD || abs(y_raw) > MOVEMENT_THRESHOLD) {
//
//                // Center of screen (160, 120) + the tilt value.
//                int screen_x = 160 + (x_raw / 2);
//                int screen_y = 120 + (y_raw / 2);
//
//                // Keep the dot inside the screen boundaries so it doesn't crash
//                if (screen_x < 0) screen_x = 0;
//                if (screen_x > 319) screen_x = 319;
//                if (screen_y < 0) screen_y = 0;
//                if (screen_y > 239) screen_y = 239;
//
//                // Draw the dot! (0x0F is typically bright white or red depending on your pixel format)
//                vga_draw_cursor(screen_x, screen_y, 0x0F);
//
//                // Print the raw values to the console for debugging
//                printf("Moved Cursor -> Screen X: %d, Screen Y: %d (Raw X: %d, Y: %d)\n", screen_x, screen_y, x_raw, y_raw);
//            }
//
//        } else {
//            printf("Hardware busy or SPI Read Error!\n");
//        }

//        // Delay to prevent spamming the SPI bus and flickering the screen (50ms = 20 FPS)
//        usleep(50000);
    }

    return 0;
}

//        altera_avalon_mutex_lock(mutex, 1);
//
//        // Test connection / Shared Memory Writes go here
//        // Test connection
//          // unsigned int val = IORD_32DIRECT(SHARED_BASE, SHARED_OFF);
//          // printf("VGA_proc: Shared Zone (32MB offset) contains: 0x%08X\n", val);
//          // if(val == 0xABC00000) {
//          //     printf("SUCCESS! Connection Established.\n");
//          //     break;
//          // }
//        altera_avalon_mutex_unlock(mutex);

