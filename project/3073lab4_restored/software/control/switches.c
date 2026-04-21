/*
 * switches.c
 *
 *  Created on: Apr 19, 2026
 *      Author: User
 */

#include "switches.h"
#include "speaker.h"
#include "system.h"
#include "altera_avalon_pio_regs.h"
#include <stdio.h>
#include <string.h>


// list of peripheral address
/*
#define Speaker_Base 0x08011110
#define SW_Base 0x080111c0
#define HEX0_BASE 0x08011190
#define HEX1_BASE 0x08011180
#define HEX2_BASE 0x08011170
#define HEX3_BASE 0x08011160
#define HEX4_BASE 0x08011150
#define HEX5_BASE 0x08011140
#define LEDS_Base 0x080111d0
*/

// check for whether hex is on or not
static int HEX_enable_bit = 0;

// for scrolling
static int scroll_counter = 0;
static int scroll_offset = 0;
const int scroll_speed = 100000; // the threshold before it resets

void HEX_enable(int state) {
	if (state & 0x01) {
		HEX_enable_bit = 1;
	}
	else {
		IOWR_ALTERA_AVALON_PIO_DATA (PIO_HEX0_BASE, 0xFF);
		IOWR_ALTERA_AVALON_PIO_DATA (PIO_HEX1_BASE, 0xFF);
		IOWR_ALTERA_AVALON_PIO_DATA (PIO_HEX2_BASE, 0xFF);
		IOWR_ALTERA_AVALON_PIO_DATA (PIO_HEX3_BASE, 0xFF);
		IOWR_ALTERA_AVALON_PIO_DATA (PIO_HEX4_BASE, 0xFF);
		IOWR_ALTERA_AVALON_PIO_DATA (PIO_HEX5_BASE, 0xFF);
		HEX_enable_bit = 0;
	}
}

void handle_switch2(int state, const char * message) {
	if ((state & 0x02) && HEX_enable_bit) {
		IOWR_ALTERA_AVALON_PIO_DATA (PIO_HEX0_BASE, translator('7'));
		IOWR_ALTERA_AVALON_PIO_DATA (PIO_HEX1_BASE, translator('6'));

		scroll_counter++;

		// shift
		if (scroll_counter >= scroll_speed) {
			char new_message [48]; // can change the size if longer messages are expected
			snprintf (new_message, sizeof(new_message), "   %s   ", message);
			int new_message_length = strlen (new_message);
			// for a 4 window HEX
			char char5 = new_message [scroll_offset];
			char char4 = new_message [scroll_offset + 1];
			char char3 = new_message [scroll_offset + 2];
			char char2 = new_message [scroll_offset + 3];

			IOWR_ALTERA_AVALON_PIO_DATA (PIO_HEX5_BASE, translator(char5));
			IOWR_ALTERA_AVALON_PIO_DATA (PIO_HEX4_BASE, translator(char4));
			IOWR_ALTERA_AVALON_PIO_DATA (PIO_HEX3_BASE, translator(char3));
			IOWR_ALTERA_AVALON_PIO_DATA (PIO_HEX2_BASE, translator(char2));

			// avoid accessing beyond the array
			scroll_offset++;
			if (scroll_offset > (new_message_length - 4)) {
				scroll_offset = 0;
			}

			// reset the counter
			scroll_counter = 0;
		}
	}
	else {
		IOWR_ALTERA_AVALON_PIO_DATA (PIO_HEX0_BASE, 0xFF);
		IOWR_ALTERA_AVALON_PIO_DATA (PIO_HEX1_BASE, 0xFF);
		IOWR_ALTERA_AVALON_PIO_DATA (PIO_HEX2_BASE, 0xFF);
		IOWR_ALTERA_AVALON_PIO_DATA (PIO_HEX3_BASE, 0xFF);
		IOWR_ALTERA_AVALON_PIO_DATA (PIO_HEX4_BASE, 0xFF);
		IOWR_ALTERA_AVALON_PIO_DATA (PIO_HEX5_BASE, 0xFF);
		scroll_counter = 0;
		scroll_offset = 0;
	}
}

void handle_switch3(int state) {
	if ((state & 0x04) && HEX_enable_bit) {
			IOWR_ALTERA_AVALON_PIO_DATA (PIO_HEX0_BASE, translator('8'));
			IOWR_ALTERA_AVALON_PIO_DATA (PIO_HEX1_BASE, translator('7'));
		}
	else {
		IOWR_ALTERA_AVALON_PIO_DATA (PIO_HEX0_BASE, 0xFF);
		IOWR_ALTERA_AVALON_PIO_DATA (PIO_HEX1_BASE, 0xFF);
	}
}

void handle_switch4(int state, int frequency) {
	if (state & 0x08) {
		play_speaker (frequency, 1);
	}
	else {
		play_speaker (frequency, 0);
	}
}

int translator(char a) {
	// for numbers
	if (a >= '0' && a <= '9') {
		// HEX translation
	    const int num_table[10] = {0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x90};
	    return num_table[a - '0'];
	}
	// for alphabets
	if (a >= 'A' && a <= 'Z') {
		// HEX translation
	    const int alpha_table[26] = {
	        0x88, 0x83, 0xC6, 0xA1, 0x86, 0x8E, 0x90, 0x89, 0xF9, 0xF1,
	        0x8A, 0xC7, 0xC8, 0xAB, 0xC0, 0x8C, 0x98, 0xAF, 0x92, 0x87,
	        0xC1, 0xE3, 0x81, 0x89, 0x91, 0xA4
	    };
	    return alpha_table[a - 'A'];
	}
	// default
	return 0xFF;
}
