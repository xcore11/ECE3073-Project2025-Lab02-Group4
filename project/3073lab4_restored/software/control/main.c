#include "io.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "switches.h"
#include "system.h"
#include "altera_avalon_pio_regs.h"

#define PB1_MASK 0x02
#define USER_MESSAGE "I Love NIOS"
#define USER_MESSAGE_LENGTH 16

#define TRAFFIC_THRESHOLD 20000
#define ACCEL_THRESHOLD 5000

int main(void)
{
    int switch_state = 0;
    int key_state = 0;
    int pb1_pressed = 0;
    int pb1_armed = 1;

    int x, y, z;
    int accel_counter = ACCEL_THRESHOLD;

    if (accel_init() != 0) {
        printf("Accelerometer init failed\n");
        while (1);
    }

    char spi_msg[USER_MESSAGE_LENGTH + 1];

    /* traffic light state */
    int traffic_counter = 0;
    int traffic_state = 0;   /* 0=green, 1=yellow, 2=red */

    spi_init_manual();

    /* initial traffic light */
    green_light(1);
    yellow_light(0);
    red_light(0);

    while (1)
    {
        switch_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_SW_BASE);
        key_state    = IORD_ALTERA_AVALON_PIO_DATA(PIO_PB_BASE);

        pb1_pressed = ((key_state & PB1_MASK) == 0);

        if (!pb1_pressed) {
            pb1_armed = 1;
        }

        if (pb1_pressed && pb1_armed)
        {
            spi_start_capture(USER_MESSAGE);
            pb1_armed = 0;

            usleep(200000);
        }

        spi_service();

        HEX_enable(switch_state);

        /* CLEAR buffer before getting new message */
        memset(spi_msg, 0, sizeof(spi_msg));

        if (spi_has_valid_message())
        {
            spi_get_message(spi_msg, sizeof(spi_msg));
            handle_switch2(switch_state, spi_msg);
        }
        else
        {
            handle_switch2(switch_state, "");
        }

        handle_switch3(switch_state);
        handle_switch4(switch_state);


        /* traffic light integration */
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

        /* Accel Integration */
        accel_counter++;

        if (accel_counter >= 5000) {
            accel_counter = 0;

            accel_read_x(&x);
            accel_read_y(&y);
            accel_read_z(&z);

            printf("Accel X = %d, Y = %d, Z = %d\n", x, y, z);
        }
    }

    return 0;
}
