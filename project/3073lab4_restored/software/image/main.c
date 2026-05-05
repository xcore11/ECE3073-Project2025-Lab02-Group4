#include <stdio.h>
#include <unistd.h>

#include "io.h"
#include "system.h"
#include "altera_avalon_pio_regs.h"
#include "spi.h"

/*
   IMG_IRQ_RX_BASE must be the PIO base connected to the image/trigger IRQ signal.

   Expected behavior:
   - IMG_IRQ_RX_BASE reads 0 when no request is present
   - IMG_IRQ_RX_BASE reads non-zero when the image/trigger processor requests SPI capture

   This code triggers SPI only on the rising edge:
   0 -> 1 causes one spi_request_text_from_esp()
   staying 1 does not repeatedly trigger
   it must return to 0 before the next trigger can be accepted
*/

#define IMG_IRQ_ACTIVE_MASK 0x01

static int img_irq_is_active(void)
{
    return (IORD_ALTERA_AVALON_PIO_DATA(IMG_IRQ_RX_BASE) & IMG_IRQ_ACTIVE_MASK) != 0;
}

int main(void)
{
    int img_irq_now = 0;
    int img_irq_prev = 0;
    unsigned int trigger_count = 0;

    printf("FPGA SPI DMA-ESP integration test started\n");
    printf("Flow: IMG_IRQ_RX rising edge -> 0x5F trigger -> wait/read 0xA1 length -> read 0xA2 packet -> store in SDRAM\n");
    printf("Waiting for IMG_IRQ_RX_BASE trigger...\n");
    fflush(stdout);

    spi_init_manual();

    while (1)
    {
        img_irq_now = img_irq_is_active();

        /* Rising edge detect: only trigger once when IRQ changes 0 -> 1 */
        if ((img_irq_now != 0) && (img_irq_prev == 0))
        {
            trigger_count++;

            printf("\n========== IMG IRQ RECEIVED ==========" "\n");
            printf("IMG_IRQ_RX rising edge detected, trigger_count=%u\n", trigger_count);
            printf("Starting ESP SPI capture/read transaction...\n");
            fflush(stdout);

            spi_request_text_from_esp();

            printf("ESP SPI transaction finished. Waiting for IMG_IRQ_RX to return low...\n");
            fflush(stdout);
        }

        img_irq_prev = img_irq_now;

        /* Small poll delay only to avoid burning CPU completely. */
        usleep(1000);
    }

    return 0;
}
