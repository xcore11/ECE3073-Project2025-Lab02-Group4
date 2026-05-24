#include "countdown.h"
#include <stdio.h>
#include <unistd.h>
#include "altera_avalon_pio_regs.h"
#include "system.h"
#include "control.h"

extern volatile int switch_state;
<<<<<<< Updated upstream
extern int translator(char a);
=======
>>>>>>> Stashed changes

static int countdown_done = 0;

void countdown_init(void) {
    countdown_done = 0;
    red_light(0);
    yellow_light(0);
    green_light(0);
    printf("Countdown ready...\n");
}

<<<<<<< Updated upstream
/* ======================================================================
   UNIVERSAL BIT-BANGED TONE GENERATOR
   ====================================================================== */
int delay_one_second_with_tone(int frequency) {
    // 1. Calculate the exact microsecond duration for half of the wave period
    int half_period = 1000000 / (frequency * 2);

    // 2. To play for exactly 1 second, loop equal to the frequency count
    int total_cycles = frequency;

    for (int i = 0; i < total_cycles; i++) {

        // INSTANT SAFETY CHECK: Check SW4 (0x10) on every single wave cycle
        // If the user flips the switch down mid-beep, abort immediately!
        if (!(switch_state & 0x10)) {
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_SPEAKER_BASE, 0); // Silence hardware immediately
            return 1; // Abort Signal
        }

        // >>> CRITICAL FIX: Directly bit-bang the hardware pin high and low <<<
=======
int delay_one_second_with_tone(int frequency) {
    int half_period = 1000000 / (frequency * 2);
    int total_cycles = frequency;

    for (int i = 0; i < total_cycles; i++) {
        // INSTANT SAFETY CHECK: Check SW4 (0x10)
        if (!(switch_state & 0x10)) {
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_SPEAKER_BASE, 0); // Silence immediately
            return 1; // Abort Signal
        }

        // Directly bit-bang the hardware pin high and low
>>>>>>> Stashed changes
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_SPEAKER_BASE, 1);
        usleep(half_period);

        IOWR_ALTERA_AVALON_PIO_DATA(PIO_SPEAKER_BASE, 0);
        usleep(half_period);
    }
<<<<<<< Updated upstream
    return 0; // Success
=======
    return 0;
>>>>>>> Stashed changes
}

int run_synchronized_countdown(void) {
    countdown_done = 0;

<<<<<<< Updated upstream
    // 1. Red Light ON + 450Hz Beep + "3" on HEX
    printf("Countdown started: 3\n");
    red_light(1); yellow_light(0); green_light(0);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, translator('3'));
    if (delay_one_second_with_tone(523)) goto abort_sequence;

    // 2. Yellow Light ON + 450Hz Beep + "2" on HEX
    printf("2\n");
    red_light(0); yellow_light(1); green_light(0);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, translator('2'));
    if (delay_one_second_with_tone(523)) goto abort_sequence;

    // 3. Green Light ON + 450Hz Beep + "1" on HEX
    printf("1\n");
    red_light(0); yellow_light(0); green_light(1);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, translator('1'));
    if (delay_one_second_with_tone(523)) goto abort_sequence;

    // 4. GO! All lights off + High-pitched 1000Hz Victory Beep + Blank HEX
    printf("GO!\n");
    red_light(0); yellow_light(0); green_light(0);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, translator(' '));
    if (delay_one_second_with_tone(1047)) goto abort_sequence;
=======
    printf("Countdown started: 3\n");
    red_light(1); yellow_light(0); green_light(0);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, translator('3'));
    if (delay_one_second_with_tone(450)) goto abort_sequence;

    printf("2\n");
    red_light(0); yellow_light(1); green_light(0);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, translator('2'));
    if (delay_one_second_with_tone(450)) goto abort_sequence;

    printf("1\n");
    red_light(0); yellow_light(0); green_light(1);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, translator('1'));
    if (delay_one_second_with_tone(450)) goto abort_sequence;

    printf("GO!\n");
    red_light(0); yellow_light(0); green_light(0);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, translator(' '));
    if (delay_one_second_with_tone(1000)) goto abort_sequence;
>>>>>>> Stashed changes

    countdown_done = 1;
    return 0; // Success

abort_sequence:
    printf("Countdown aborted!\n");
    red_light(0); yellow_light(0); green_light(0);
<<<<<<< Updated upstream
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_SPEAKER_BASE, 0); // Ensure speaker is OFF
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, 0xFF); // Blank the HEX
=======
    play_speaker(1000, 0);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, 0xFF);
>>>>>>> Stashed changes
    return 1; // Aborted
}

int is_countdown_finished(void) {
    return countdown_done;
}
