#include "soundeffects.h"
#include <unistd.h>  // For usleep()
#include <stdlib.h>  // For rand()
#include "system.h"
#include "control.h"


void sound_init(void) {
    play_speaker(0, 0);
}


// to allow the software counter to reach its threshold and toggle.
void play_tone_blocking(int frequency, int duration_us) {
    int elapsed = 0;
    while (elapsed < duration_us) {
        play_speaker(frequency, 1);
        usleep(2); // The exact loop interval your driver's formula expects
        elapsed += 2;
    }
}

/* =========================================================
   1. End Screen (Died Snake game)
   ========================================================= */
void sfx_laser_shoot(void) {
    int freq;
    for (freq = 2500; freq > 500; freq -= 50) {
        // Play each frequency step for 1500 microseconds
        play_tone_blocking(freq, 1500);
    }
    play_speaker(0, 0);
}

/* =========================================================
   2. EXPLOSION / CRASH SOUND
   ========================================================= */
void sfx_explosion(void) {
    int i;
    for (i = 0; i < 20; i++) {
        int noise_freq = 100 + (rand() % 500);
        int step_duration = 2000 + (rand() % 5000);
        play_tone_blocking(noise_freq, step_duration);
    }

    int rumble;
    for(rumble = 200; rumble > 50; rumble -= 10) {
        play_tone_blocking(rumble, 4000);
    }
    play_speaker(0, 0);
}

/* =========================================================
   3. PORTAL (FOR SNAKE)
   ========================================================= */
void sfx_portal(void) {
    play_tone_blocking(880, 50000);
    play_tone_blocking(1108, 50000);
    play_tone_blocking(1318, 50000);
    play_tone_blocking(1760, 100000);
    play_speaker(0, 0);
}

/* =========================================================
   4. MENU BLIP / SELECT
   ========================================================= */
void sfx_menu_blip(void) {
    play_tone_blocking(2400, 20000); // 20ms blip
    play_speaker(0, 0);
}

/* =========================================================
   5. ERROR / INVALID ACTION BUZZ (BOAT)
   ========================================================= */
void sfx_error_buzz(void) {
    play_tone_blocking(400, 100000);
    play_speaker(0, 0);
    usleep(50000);
    play_tone_blocking(400, 150000);
    play_speaker(0, 0);
}

/* =========================================================
   6. SNAKE APPLE
   ========================================================= */
void sfx_eat_apple(void){
	    // Note 1: C6
	    play_tone_blocking(1046, 15000);
	    // Note 2: C7
	    play_tone_blocking(2094, 20000);
	    play_speaker(0, 0);
}

/* =========================================================
   7. End Screen (Boat and snake)
   ========================================================= */
void sfx_end_screen(void) {
    // Note 1: G
    play_tone_blocking(1568, 25000); // Change to 523 for lower octave

    // Note 2: B
    play_tone_blocking(1976, 25000); // Change to 659 for lower octave

    // Note 3: D
    play_tone_blocking(2250, 25000); // Change to 784 for lower octave

    // Note 4: G
    play_tone_blocking(3136, 25000);
    usleep(100000);

    // Note 5: D
    play_tone_blocking(2250, 25000);

    // Note 6: G
    play_tone_blocking(3136, 50000);

    // Clean cutoff
    play_speaker(0, 0);
}

/* =========================================================
   7. Change Arsenal (Boat)
   ========================================================= */
void sfx_change(void){
    play_tone_blocking(2094, 20000);
    play_speaker(0, 0);
}
