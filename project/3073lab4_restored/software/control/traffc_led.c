/*
 * traffic_led.c
 */
#include "altera_avalon_pio_regs.h"
#include <stdio.h>
#include "system.h"
#include "control.h"
#include <unistd.h>
#include "soundeffects.h"

// Bit 2 (Red), Bit 1 (Yellow), Bit 0 (Green)
static int LED_state = 0b000;

void red_light (int on_off) {
    if (on_off) {
        LED_state |= 0b100;  // Sets Bit 2 (Red ON)
    } else {
        LED_state &= ~0b100; // FIX: Clears Bit 2 (Red OFF)
    }
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_LED_MODULE_BASE, LED_state);
}

void yellow_light (int on_off) {
    if (on_off) {
        LED_state |= 0b010;  // Sets Bit 1 (Yellow ON)
    } else {
        LED_state &= ~0b010; // Clears Bit 1 (Yellow OFF)
    }
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_LED_MODULE_BASE, LED_state);
}

void green_light (int on_off) {
    if (on_off) {
        LED_state |= 0b001;  // Sets Bit 0 (Green ON)
    } else {
        LED_state &= ~0b001; // FIX: Clears Bit 0 (Green OFF)
    }
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_LED_MODULE_BASE, LED_state);
}

void game_over(int trigger)
{
	if (trigger){
		int i;
		for (i = 0; i < 5; i++){
			red_light(1);
			usleep(100000);
			red_light(0);
			yellow_light(1);
			usleep(100000);
			yellow_light(0);
			green_light(1);
			usleep(100000);
			green_light(0);
			usleep(100000);
		}
		red_light(1);
		sfx_end_screen();
	}
}

void eat_apple(int trigger)
{
	if (trigger){
		green_light(1);
		sfx_eat_apple();
		usleep(40000);
		green_light(0);

	}
}

void explosion(int trigger){
	if (trigger){
		int i;
		for (i = 0; i < 5; i++){
			yellow_light(1);
			usleep(100000);
			yellow_light(0);
			usleep(100000);
		}
		sfx_explosion();
	}
}

void portal(int trigger){
	if (trigger){
		int i;
		for (i = 0; i < 5; i++){
			red_light(1);
			usleep(50000);
			red_light(0);
			green_light(1);
			usleep(50000);
			green_light(0);
		}
		sfx_portal();
	}
}

void miss (int trigger){
	int i;
	for (i = 0; i < 5; i++){
		red_light(1);
		usleep(50000);
		red_light(0);
		yellow_light(1);
		usleep(50000);
		yellow_light(0);
	}
	sfx_error_buzz();
}
