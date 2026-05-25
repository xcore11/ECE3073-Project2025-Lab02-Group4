/*
 * traffic_led.c
 *
 *  Created on: Apr 20, 2026
 *      Author: User
 */
#include "altera_avalon_pio_regs.h"
#include <stdio.h>
#include <unistd.h>
#include "system.h"
#include "control.h"
#include "soundeffects.h"

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

void game_over(int trigger)
{
    int i;

    if (!trigger)
        return;

    for (i = 0; i < 5; i++)
    {
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

void eat_apple(int trigger)
{
    if (!trigger)
        return;

    green_light(1);
    sfx_eat_apple();
    usleep(40000);
    green_light(0);
}

void explosion(int trigger)
{
    int i;

    if (!trigger)
        return;

    for (i = 0; i < 5; i++)
    {
        yellow_light(1);
        usleep(100000);
        yellow_light(0);
        usleep(100000);
    }

    sfx_explosion();
}

void portal(int trigger)
{
    int i;

    if (!trigger)
        return;

    for (i = 0; i < 5; i++)
    {
        red_light(1);
        usleep(50000);
        red_light(0);

        green_light(1);
        usleep(50000);
        green_light(0);
    }

    sfx_portal();
}

void miss(int trigger)
{
    int i;

    if (!trigger)
        return;

    for (i = 0; i < 5; i++)
    {
        red_light(1);
        usleep(50000);
        red_light(0);

        yellow_light(1);
        usleep(50000);
        yellow_light(0);
    }

    sfx_error_buzz();
}
