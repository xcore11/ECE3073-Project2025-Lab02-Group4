#include "io.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "system.h"
#include "altera_avalon_pio_regs.h"
#include "control.h"
#include "soundeffects.h"

#define USER_MESSAGE "I Love NIOS"
#define USER_MESSAGE_LENGTH 16

#define TRAFFIC_THRESHOLD 20000
#define ACCEL_THRESHOLD 5000
extern handle_snake_game_switch();
extern handle_switch9();

int main(void)
{
    control_shared_flags_init();
    // Setup Traffic Light
    int traffic_counter = 0;
    int traffic_state = 0;   /* 0=green, 1=yellow, 2=red */

    /* initial traffic light */
    green_light(0);
    yellow_light(0);
    red_light(0);

    // Setup the switch and key interrupts
    sound_init();
    switch_setup();
    key_setup();
    img_rx_setup();
    vga_rx_setup();
    int last_sw9_state = 0;
    int last_tilt_state = 0;

    while (1)
    {
    	// Key Operations
    	handle_key1();
    	handle_key2();

        // Flag Switch and Reset HEX
        HEX_enable();

        // Handle CPU and Speaker switch
        handle_switch2("TEST123");
        handle_switch3();
        handle_switch4();
        handle_snake_game_switch();
        handle_switch9();

        int live_switches = IORD_ALTERA_AVALON_PIO_DATA(PIO_SW_BASE);

        // Extract just the value of Switch 9 (bit 9)
        int current_sw9_state = (live_switches & (1 << 9)) ? 1 : 0;

        // If Switch 9 is currently UP (1) AND was previously DOWN (0)
        if (current_sw9_state == 1 && last_sw9_state == 0) {
            sfx_laser_shoot();
            }
        last_sw9_state = current_sw9_state;

        /* GAME BEHAVIOUR*/
        // 1. Check if the VGA processor says an apple was eaten
        if (IORD_32DIRECT(SHARED_FLAGS_BASE, FLAG_EAT_APPLE) == 1) {
        	eat_apple(1);
            // CLEAR THE FLAG so it doesn't trigger forever
            IOWR_32DIRECT(SHARED_FLAGS_BASE, FLAG_EAT_APPLE, 0);
            }

        // 2. Check if the VGA processor says the game is over (SNAKE)
        if (IORD_32DIRECT(SHARED_FLAGS_BASE, FLAG_GAME_OVER) == 1) {
        	game_over(1);
            // CLEAR THE FLAG
            IOWR_32DIRECT(SHARED_FLAGS_BASE, FLAG_GAME_OVER, 0);
            green_light(0);
            yellow_light(0);
            red_light(0);
           }

        // 3. Check if the VGA processor says Snake goes into portal
        if (IORD_32DIRECT(SHARED_FLAGS_BASE, FLAG_PORTAL) == 1) {
        	portal(1);
            // CLEAR THE FLAG
            IOWR_32DIRECT(SHARED_FLAGS_BASE, FLAG_PORTAL, 0);
            green_light(0);
            yellow_light(0);
            red_light(0);
           }

        // 4. Check if the VGA processor says the ship kaboom (Ship)
        if (IORD_32DIRECT(SHARED_FLAGS_BASE, FLAG_EXPLOSION) == 1) {
        	explosion(1);
            // CLEAR THE FLAG
            IOWR_32DIRECT(SHARED_FLAGS_BASE, FLAG_EXPLOSION, 0);
            green_light(0);
            yellow_light(0);
            red_light(0);
           }

        // 5. Check if the VGA processor says the ship miss (Ship)
        if (IORD_32DIRECT(SHARED_FLAGS_BASE, FLAG_MISS) == 1) {
        	portal(1);
            // CLEAR THE FLAG
            IOWR_32DIRECT(SHARED_FLAGS_BASE, FLAG_MISS, 0);
            green_light(0);
            yellow_light(0);
            red_light(0);
           }
        // 5. Check if the VGA processor says the ship change arsenal
        if (IORD_32DIRECT(SHARED_FLAGS_BASE, FLAG_CHANGE_ARSENAL) == 1) {
            sfx_menu_blip();
            IOWR_32DIRECT(SHARED_FLAGS_BASE, FLAG_CHANGE_ARSENAL, 0);
        }

        /* TILT STATE*/
        // Read the tilt state from 0x44
        int current_tilt_state = IORD_32DIRECT(SHARED_FLAGS_BASE, FLAG_ACCEL_MENU);

        // TILT UP
        if (current_tilt_state == 1 && last_tilt_state != 1) {
           red_light(0);
           green_light(1);
           sfx_menu_blip();
           }
        // TILT DOWN
        else if (current_tilt_state == -1 && last_tilt_state != -1) {
           green_light(0);
           red_light(1);
           sfx_menu_blip();
           }
        // NEUTRAL
        else if (current_tilt_state == 0 && last_tilt_state != 0) {
           red_light(0);
           green_light(0);
           }
    }
    return 0;
}
