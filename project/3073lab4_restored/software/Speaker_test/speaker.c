#include "system.h"
#include "io.h"
#include "altera_avalon_pio_regs.h"


// Delay
void delay(int loops) {
    volatile int i;
    for (i = 0; i < loops; i++) {
        // Leave it be
    }
}

// Play a random tone
void test_speaker(int frequency, int time_ms){
	// Convert time to clock cycles
	int clock_cycles = (frequency * time_ms)/1000;
	for (int i=0; i < clock_cycles; i++){
		// Toggle speaker
		IOWR_ALTERA_AVALON_PIO_DATA(PIO_SPEAKER_BASE, 1);
		delay(5);

		// Turn speaker OFF
		IOWR_ALTERA_AVALON_PIO_DATA(PIO_SPEAKER_BASE, 0);
		delay(5);
	}
}

int main(){
	while (1){
		IOWR_ALTERA_AVALON_PIO_DATA(PIO_SPEAKER_BASE, 0);
		test_speaker(440, 3000);
		IOWR_ALTERA_AVALON_PIO_DATA(PIO_SPEAKER_BASE, 0);
		test_speaker(200, 5000);
	}
	return 0;
}
