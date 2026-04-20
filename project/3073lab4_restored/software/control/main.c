#include "io.h"
#include <unistd.h>
#include "switches.h"
#include "speaker.h"
#include "system.h"
#include "altera_avalon_pio_regs.h"
#include <stdio.h>

// HEX base addresses
/*
#define HEX0_BASE 0x08011190
#define HEX1_BASE 0x08011180
#define HEX2_BASE 0x08011170
#define HEX3_BASE 0x08011160
#define HEX4_BASE 0x08011150
#define HEX5_BASE 0x08011140
*/

// Active-low 7-segment codes
#define SEG_1 0xF9
#define SEG_2 0xA4
#define SEG_3 0xB0
#define SEG_4 0x99
#define SEG_5 0x92
#define SEG_6 0x82

// Switch base addresses
// #define SW_Base 0x080111c0

int main(void)
{
	printf ("test");
	int switch_state = 0;
    while (1)
    {
    	switch_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_SW_BASE);
    	HEX_enable (switch_state);
    	handle_switch2 (switch_state, "ECE3073");
    	handle_switch3 (switch_state);
    	handle_switch4 (switch_state, 1000);
    }

    return 0;
}
