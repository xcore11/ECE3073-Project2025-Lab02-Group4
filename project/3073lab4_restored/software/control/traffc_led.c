/*
 * traffic_led.c
 *
 *  Created on: Apr 20, 2026
 *      Author: User
 */
#include "switches.h"
#include "altera_avalon_pio_regs.h"
#include <stdio.h>
#include "system.h"

static int LED_state = 0b000;

void red_light (int on_off) {
	if (on_off) {
		LED_state = (LED_state | 0b001);
		IOWR_ALTERA_AVALON_PIO_DATA (PIO_LED_MODULE_BASE, (LED_state));
	}
	else {
		LED_state = (LED_state & 0b110);
		IOWR_ALTERA_AVALON_PIO_DATA (PIO_LED_MODULE_BASE, (LED_state));
	}
}

void yellow_light (int on_off) {
	if (on_off) {
		LED_state = (LED_state | 0b010);
		IOWR_ALTERA_AVALON_PIO_DATA (PIO_LED_MODULE_BASE, (LED_state));
	}
	else {
		LED_state = (LED_state & 0b101);
		IOWR_ALTERA_AVALON_PIO_DATA (PIO_LED_MODULE_BASE, (LED_state));
	}
}
void green_light (int on_off) {
	if (on_off) {
		LED_state = (LED_state | 0b100);
		IOWR_ALTERA_AVALON_PIO_DATA (PIO_LED_MODULE_BASE, (LED_state));
	}
	else {
		LED_state = (LED_state & 0b011);
		IOWR_ALTERA_AVALON_PIO_DATA (PIO_LED_MODULE_BASE, (LED_state));
	}
}
