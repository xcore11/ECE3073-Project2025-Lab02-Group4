#include "io.h"
#include <unistd.h>
#include "switches.h"
#include "system.h"
#include "altera_avalon_pio_regs.h"
#include <stdio.h>

#define PIO_KEY_BASE 0x04011100   /* change this if your PB base is different */

int main(void)
{
    printf("test");

    int switch_state = 0;
    int pb_state = 0;
    const char *scroll_message = "ECE3073";

    while (1)
    {
        switch_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_SW_BASE);
        pb_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_KEY_BASE);

        /* PB1 only changes the scrolling text used by switch 2 */
        if (pb_state & PB1_MASK)
        {
            scroll_message = "ERROR";
        }
        else
        {
            scroll_message = "ECE3073";
        }

        HEX_enable(switch_state);
        handle_switch2(switch_state, scroll_message);
        handle_switch3(switch_state);
        handle_switch4(switch_state);
    }

    return 0;
}
