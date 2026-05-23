/*
 * speaker.c
 *
 *  Created on: Apr 20, 2026
 *      Author: User
 */

#include "system.h"
#include "io.h"
#include "altera_avalon_pio_regs.h"

#include "control.h"

static int speaker_current = 0; // current state of the speaker
static int toggle_counter = 0; // counter for toggling
static int frequency_threshold = 0; // threshold before toggling and resetting counter
static int frequency_prev = 0; // remembers previous frequency to avoid redundant calculations


// Play a given tone
void play_speaker (int frequency, int on_off){

	// check if frequency is new and valid
	if (frequency > 0) {
		if (frequency != frequency_prev)
		{
			// updates the required parameters
			frequency_threshold = (500000/(frequency*2));
			frequency_prev = frequency;
		}
	}
	else {
		frequency_prev = frequency;
	}

	// toggles if on
	if (on_off && (frequency > 0)){
		toggle_counter++;
		if (toggle_counter >= frequency_threshold){
			speaker_current = (!speaker_current);
			IOWR_ALTERA_AVALON_PIO_DATA(PIO_SPEAKER_BASE, speaker_current);
			toggle_counter = 0;
		}
	}
	// silences if off
	else {
		IOWR_ALTERA_AVALON_PIO_DATA(PIO_SPEAKER_BASE, 0);
		speaker_current = 0;
		toggle_counter = 0;
	}
}

