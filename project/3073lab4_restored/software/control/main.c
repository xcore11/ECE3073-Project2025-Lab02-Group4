#include "io.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "system.h"
#include "altera_avalon_pio_regs.h"
#include "control.h"

#define USER_MESSAGE "I Love NIOS"
#define USER_MESSAGE_LENGTH 16

#define TRAFFIC_THRESHOLD 20000
#define ACCEL_THRESHOLD 5000

int main(void)
{
    control_shared_flags_init();
    // Setup Traffic Light
    int traffic_counter = 0;
    int traffic_state = 0;   /* 0=green, 1=yellow, 2=red */

    /* initial traffic light */
    green_light(1);
    yellow_light(0);
    red_light(0);

    // Setup the switch and key interrupts
    switch_setup();
    key_setup();
    img_rx_setup();
    vga_rx_setup();

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

        // Traffic Light operation
        traffic_counter++;
        if (traffic_counter >= TRAFFIC_THRESHOLD)
        {
            if (traffic_state == 0) {
                green_light(0);
                yellow_light(1);
                red_light(0);
                traffic_state = 1;
            }
            else if (traffic_state == 1) {
                green_light(0);
                yellow_light(0);
                red_light(1);
                traffic_state = 2;
            }
            else {
                green_light(1);
                yellow_light(0);
                red_light(0);
                traffic_state = 0;
            }

            traffic_counter = 0;
        }
    }
    return 0;
}
