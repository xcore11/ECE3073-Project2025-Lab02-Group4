#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#include "io.h"
#include "system.h"
#include "altera_avalon_pio_regs.h"
#include "sys/alt_irq.h"

#include "spi.h"
#include "decoder.h"

/* ============================================================
   IMAGE PROCESSOR MAIN - EDGE SAFE PACKET READ

   IMG_IRQ_RX:
       Session-start button input.

   ESP_DATA_READY:
       ESP -> FPGA packet-ready signal.

   Important behaviour:
       - FPGA sends 0x5F once after button.
       - FPGA reads ONE packet when ESP_DATA_READY is high.
       - FPGA does NOT spam-drain multiple packets inside one tight loop.
       - FPGA does NOT depend only on IRQ edge; it also polls level.
       - No long 50 ms sleep after successful packet.

   This matches the ESP RTOS packet queue better than the old drain loop.
   ============================================================ */

#define SESSION_BUTTON_MASK              0x01
#define ESP_DATA_READY_ACTIVE_MASK       0x01

#define DEBUG_DECODER_PRINT              1

/*
   Small guard after each packet read.
   This gives the ESP SPI DMA task time to finish the previous transaction
   and present/promote the next queued packet before FPGA asks again.
*/
#define FPGA_AFTER_PACKET_GUARD_US       5000

static volatile int session_button_pending = 0;
static volatile int esp_data_ready_pending = 0;
static int session_started = 0;

/* ============================================================
   DECODER RESULT PRINT
   ============================================================ */

static void print_decoder_result(int result)
{
#if DEBUG_DECODER_PRINT
    if (result == DECODER_OK_SENT)
    {
        printf("Decoder: snake command sent to SDRAM mailbox\n");
    }
    else if (result == DECODER_OK_WAITING_PORTAL)
    {
        printf("Decoder: portal stored, waiting for other portal\n");
    }
    else if (result == DECODER_NO_COMMAND)
    {
        printf("Decoder: no snake command detected\n");
    }
    else if (result == DECODER_ERR_BAD_FORMAT)
    {
        printf("Decoder error: bad command format -> reset stream\n");
    }
    else if (result == DECODER_ERR_OUT_OF_RANGE)
    {
        printf("Decoder error: coordinate out of snake grid range -> reset stream\n");
    }
    else if (result == DECODER_ERR_MAILBOX_BUSY)
    {
        printf("Decoder error: snake mailbox busy\n");
    }
    else
    {
        printf("Decoder error: unknown result %d -> reset stream\n", result);
    }
#else
    (void)result;
#endif
}

/* ============================================================
   SESSION START BUTTON IRQ
   ============================================================ */

static void session_button_isr(void *context)
{
    (void)context;

    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(
        IMG_IRQ_RX_BASE,
        SESSION_BUTTON_MASK
    );

    if (!session_started)
    {
        session_button_pending = 1;
    }
}

static void session_button_setup(void)
{
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(IMG_IRQ_RX_BASE, 0x0);
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(IMG_IRQ_RX_BASE, SESSION_BUTTON_MASK);

#if defined(IMG_IRQ_RX_IRQ) && defined(IMG_IRQ_RX_IRQ_INTERRUPT_CONTROLLER_ID)
    alt_ic_isr_register(
        IMG_IRQ_RX_IRQ_INTERRUPT_CONTROLLER_ID,
        IMG_IRQ_RX_IRQ,
        session_button_isr,
        NULL,
        NULL
    );
#else
    alt_irq_register(
        IMG_IRQ_RX_IRQ,
        NULL,
        session_button_isr
    );
#endif

    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(IMG_IRQ_RX_BASE, SESSION_BUTTON_MASK);
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(IMG_IRQ_RX_BASE, SESSION_BUTTON_MASK);
}

static int session_button_is_high(void)
{
    return (
        IORD_ALTERA_AVALON_PIO_DATA(IMG_IRQ_RX_BASE)
        & SESSION_BUTTON_MASK
    ) != 0;
}

/* ============================================================
   ESP DATA READY IRQ
   ============================================================ */

static void esp_data_ready_isr(void *context)
{
    (void)context;

    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(
        ESP_DATA_READY_BASE,
        ESP_DATA_READY_ACTIVE_MASK
    );

    if (session_started)
    {
        esp_data_ready_pending = 1;
    }
}

static void esp_data_ready_setup(void)
{
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(ESP_DATA_READY_BASE, 0x0);
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(ESP_DATA_READY_BASE, ESP_DATA_READY_ACTIVE_MASK);

#if defined(ESP_DATA_READY_IRQ) && defined(ESP_DATA_READY_IRQ_INTERRUPT_CONTROLLER_ID)
    alt_ic_isr_register(
        ESP_DATA_READY_IRQ_INTERRUPT_CONTROLLER_ID,
        ESP_DATA_READY_IRQ,
        esp_data_ready_isr,
        NULL,
        NULL
    );
#else
    alt_irq_register(
        ESP_DATA_READY_IRQ,
        NULL,
        esp_data_ready_isr
    );
#endif

    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(ESP_DATA_READY_BASE, ESP_DATA_READY_ACTIVE_MASK);
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(ESP_DATA_READY_BASE, ESP_DATA_READY_ACTIVE_MASK);
}

static int esp_data_ready_is_high(void)
{
    return (
        IORD_ALTERA_AVALON_PIO_DATA(ESP_DATA_READY_BASE)
        & ESP_DATA_READY_ACTIVE_MASK
    ) != 0;
}

/* ============================================================
   OPTIONAL FPGA -> VGA / OLD ACK LINE
   ============================================================ */

static void img_irq_tx_set_true(void)
{
#ifdef IMG_IRQ_TX_BASE
    IOWR_ALTERA_AVALON_PIO_DATA(IMG_IRQ_TX_BASE, 1);
#endif
}

static void img_irq_tx_set_false(void)
{
#ifdef IMG_IRQ_TX_BASE
    IOWR_ALTERA_AVALON_PIO_DATA(IMG_IRQ_TX_BASE, 0);
#endif
}

/* ============================================================
   READ EXACTLY ONE ESP PACKET
   ============================================================ */

static int read_one_esp_packet(void)
{
    const char *rx_text;
    uint32_t packet_len;
    int decoder_result;

    img_irq_tx_set_false();

    spi_request_text_from_esp();
    packet_len = spi_get_latest_packet_length();

    if (packet_len == 0)
    {
        printf("SPI read failed or no packet\n");
        fflush(stdout);
        return 0;
    }

    rx_text = spi_get_latest_text();

    if (rx_text == NULL || rx_text[0] == '\0')
    {
        printf("SPI packet_len=%lu, text empty\n", (unsigned long)packet_len);
        fflush(stdout);
        img_irq_tx_set_true();
        return 1;
    }

    printf("SPI packet_len=%lu, text=[%s]\n", (unsigned long)packet_len, rx_text);
    fflush(stdout);

    decoder_result = decoder_decode_and_store_snake_command(rx_text);
    print_decoder_result(decoder_result);

    /*
       If the stream is malformed, reset decoder so the next SNAKE header can
       recover cleanly instead of staying poisoned forever.
    */
    if (decoder_result < 0 && decoder_result != DECODER_ERR_MAILBOX_BUSY)
    {
        decoder_reset_stream();
    }

    img_irq_tx_set_true();
    return 1;
}

/* ============================================================
   MAIN
   ============================================================ */

int main(void)
{
    printf("========== IMAGE PROCESSOR MAIN ==========\n");
    printf("SPI edge-safe packet read + streaming snake decoder enabled\n");
    printf("IMG_IRQ_RX = session start button\n");
    printf("ESP_DATA_READY = packet ready trigger\n");
    printf("==========================================\n");
    fflush(stdout);

    spi_init_manual();
    img_irq_tx_set_false();
    decoder_reset_stream();

    session_button_setup();
    esp_data_ready_setup();

    printf("Image processor ready - press IMG_IRQ_RX button once to start ESP session\n");
    fflush(stdout);

    while (1)
    {
        if (session_button_pending && !session_started)
        {
            session_button_pending = 0;
            session_started = 1;

            printf("SESSION START button received. Sending 0x5F to ESP...\n");
            fflush(stdout);

            spi_send_session_start();

            printf("ESP session start command sent. Waiting for ESP_DATA_READY.\n");
            fflush(stdout);

            while (session_button_is_high())
            {
                usleep(1000);
            }

            IOWR_ALTERA_AVALON_PIO_EDGE_CAP(IMG_IRQ_RX_BASE, SESSION_BUTTON_MASK);
            IOWR_ALTERA_AVALON_PIO_EDGE_CAP(ESP_DATA_READY_BASE, ESP_DATA_READY_ACTIVE_MASK);
        }

        if (session_started)
        {
            /*
               Use IRQ as a wake-up, but also poll the level.
               This avoids missing a short edge if ESP promotes another packet.

               Important: read only ONE packet here. Do not tight-loop drain.
            */
            if (esp_data_ready_pending || esp_data_ready_is_high())
            {
                esp_data_ready_pending = 0;

                if (esp_data_ready_is_high())
                {
                    printf("ESP_DATA_READY high, reading one packet\n");
                    fflush(stdout);
                    read_one_esp_packet();

                    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(
                        ESP_DATA_READY_BASE,
                        ESP_DATA_READY_ACTIVE_MASK
                    );

                    /* Give ESP DMA task time to promote/present next packet. */
                    usleep(FPGA_AFTER_PACKET_GUARD_US);
                    continue;
                }
            }
        }

        usleep(500);
    }

    return 0;
}
