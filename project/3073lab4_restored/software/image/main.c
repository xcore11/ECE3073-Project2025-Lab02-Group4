#include <stdio.h>
#include <unistd.h>

#include "io.h"
#include "system.h"
#include "altera_avalon_pio_regs.h"
#include "sys/alt_irq.h"

#include "spi.h"

#define IMG_IRQ_ACTIVE_MASK 0x01

static volatile unsigned int trigger_count = 0;
static volatile int img_irq_pending = 0;

static void img_irq_rx_isr(void* context)
{
    /*
       Clear/acknowledge PIO edge interrupt.
    */
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(IMG_IRQ_RX_BASE, IMG_IRQ_ACTIVE_MASK);

    /*
       Tell main loop to do the slow work.
    */
    img_irq_pending = 1;
}

static void img_irq_rx_setup(void)
{
    /*
       Disable first while configuring.
    */
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(IMG_IRQ_RX_BASE, 0x0);

    /*
       Clear pending edge capture.
    */
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(IMG_IRQ_RX_BASE, IMG_IRQ_ACTIVE_MASK);

    /*
       Register ISR.
    */
    alt_ic_isr_register(
        IMG_IRQ_RX_IRQ_INTERRUPT_CONTROLLER_ID,
        IMG_IRQ_RX_IRQ,
        img_irq_rx_isr,
        NULL,
        NULL
    );

    /*
       Clear again after registering, then enable.
    */
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(IMG_IRQ_RX_BASE, IMG_IRQ_ACTIVE_MASK);
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(IMG_IRQ_RX_BASE, IMG_IRQ_ACTIVE_MASK);
}

static void img_irq_tx_set_true(void)
{
    /*
       Tell the other side SPI/text processing is done.
    */
    IOWR_ALTERA_AVALON_PIO_DATA(IMG_IRQ_TX_BASE, 1);
}

static void img_irq_tx_set_false(void)
{
    /*
       Reset TX trigger back to low.
    */
    IOWR_ALTERA_AVALON_PIO_DATA(IMG_IRQ_TX_BASE, 0);
}

int main(void)
{
    printf("FPGA SPI DMA-ESP integration test started\n");
    printf("Setting up SPI and IMG_IRQ_RX interrupt...\n");
    fflush(stdout);

    spi_init_manual();

    /*
       Make sure IMG_IRQ_TX starts low.
    */
    img_irq_tx_set_false();

    img_irq_rx_setup();

    printf("Ready. Waiting for IMG_IRQ_RX interrupt...\n");
    fflush(stdout);

    while (1)
    {
        if (img_irq_pending)
        {
            img_irq_pending = 0;
            trigger_count++;

            printf("interrupted, trigger_count=%u\n", trigger_count);
            fflush(stdout);

            /*
               Clear done signal before starting new SPI transaction.
            */
            img_irq_tx_set_false();

            /*
               Do SPI request.
            */
            spi_request_text_from_esp();

            /*
               SPI is done, set IMG_IRQ_TX to true.
            */
            img_irq_tx_set_true();

            /*
               Optional: wait until the trigger signal goes low before accepting another.
               This prevents repeated handling while the source is still high.
            */
            while ((IORD_ALTERA_AVALON_PIO_DATA(IMG_IRQ_RX_BASE) & IMG_IRQ_ACTIVE_MASK) != 0)
            {
                usleep(1000);
            }

            /*
               Clear any edge that happened while SPI was running.
            */
            IOWR_ALTERA_AVALON_PIO_EDGE_CAP(IMG_IRQ_RX_BASE, IMG_IRQ_ACTIVE_MASK);
        }
    }

    return 0;
}
