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
   IMAGE PROCESSOR MAIN

   IMG_IRQ_RX:
       Used as SESSION START button.

   ESP_DATA_READY:
       Used as ESP -> FPGA packet-ready interrupt.

   Runtime flow:
       1. Press IMG_IRQ_RX once
       2. FPGA sends 0x5F to ESP
       3. ESP raises ESP_DATA_READY when packet is ready
       4. FPGA reads SPI packet
       5. FPGA gets latest text chunk
       6. decoder.c accumulates chunks and writes snake SDRAM mailbox
   ============================================================ */

#define SESSION_BUTTON_MASK           0x01
#define ESP_DATA_READY_ACTIVE_MASK    0x01

#define DEBUG_DECODER_PRINT           1

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
        printf("Decoder error: bad command format\n");
    }
    else if (result == DECODER_ERR_OUT_OF_RANGE)
    {
        printf("Decoder error: coordinate out of snake grid range\n");
    }
    else if (result == DECODER_ERR_MAILBOX_BUSY)
    {
        printf("Decoder error: snake mailbox busy\n");
    }
    else
    {
        printf("Decoder error: unknown result %d\n", result);
    }
#else
    (void)result;
#endif
}

/* ============================================================
   SESSION START BUTTON IRQ
   IMG_IRQ_RX is used as the session-start button.
   ============================================================ */

static void session_button_isr(void *context)
{
    (void)context;

    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(
        IMG_IRQ_RX_BASE,
        SESSION_BUTTON_MASK
    );

    /*
       Only first button press matters.
       After session_started = 1, ignore button.
    */
    if (!session_started)
    {
        session_button_pending = 1;
    }
}

static void session_button_setup(void)
{
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(
        IMG_IRQ_RX_BASE,
        0x0
    );

    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(
        IMG_IRQ_RX_BASE,
        SESSION_BUTTON_MASK
    );

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

    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(
        IMG_IRQ_RX_BASE,
        SESSION_BUTTON_MASK
    );

    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(
        IMG_IRQ_RX_BASE,
        SESSION_BUTTON_MASK
    );
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
   ESP_DATA_READY is ESP -> FPGA packet-ready signal.
   ============================================================ */

static void esp_data_ready_isr(void *context)
{
    (void)context;

    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(
        ESP_DATA_READY_BASE,
        ESP_DATA_READY_ACTIVE_MASK
    );

    /*
       Only accept packet-ready edges after session starts.
    */
    if (session_started)
    {
        esp_data_ready_pending = 1;
    }
}

static void esp_data_ready_setup(void)
{
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(
        ESP_DATA_READY_BASE,
        0x0
    );

    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(
        ESP_DATA_READY_BASE,
        ESP_DATA_READY_ACTIVE_MASK
    );

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

    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(
        ESP_DATA_READY_BASE,
        ESP_DATA_READY_ACTIVE_MASK
    );

    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(
        ESP_DATA_READY_BASE,
        ESP_DATA_READY_ACTIVE_MASK
    );
}

static int esp_data_ready_is_high(void)
{
    return (
        IORD_ALTERA_AVALON_PIO_DATA(ESP_DATA_READY_BASE)
        & ESP_DATA_READY_ACTIVE_MASK
    ) != 0;
}

/* ============================================================
   FPGA -> VGA / OLD ACK LINE
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
   MAIN
   ============================================================ */

int main(void)
{
    const char *rx_text;
    uint32_t packet_len;
    int decoder_result;

    printf("========== IMAGE PROCESSOR MAIN ==========\n");
    printf("SPI packet receive + streaming snake decoder enabled\n");
    printf("IMG_IRQ_RX = session start button\n");
    printf("ESP_DATA_READY = packet ready interrupt\n");
    printf("==========================================\n");
    fflush(stdout);

    /*
       Init SPI and clear SDRAM text/image/status buffers.
    */
    spi_init_manual();

    /*
       Clear output IRQ / ACK line.
    */
    img_irq_tx_set_false();

    /*
       Optional: reset decoder stream at boot.
    */
    decoder_reset_stream();

    /*
       Setup both interrupts.
    */
    session_button_setup();
    esp_data_ready_setup();

    printf("Image processor ready - press IMG_IRQ_RX button once to start ESP session\n");
    fflush(stdout);

    while (1)
    {
        /*
           First button press starts realtime session.
           This sends 0x5F to ESP once.
        */
        if (session_button_pending && !session_started)
        {
            session_button_pending = 0;
            session_started = 1;

            printf("SESSION START button received. Sending 0x5F to ESP...\n");
            fflush(stdout);

            spi_send_session_start();

            printf("ESP session start command sent. Waiting for ESP_DATA_READY edges.\n");
            fflush(stdout);

            /*
               Wait for button signal to release so old edges do not retrigger.
            */
            while (session_button_is_high())
            {
                usleep(1000);
            }

            IOWR_ALTERA_AVALON_PIO_EDGE_CAP(
                IMG_IRQ_RX_BASE,
                SESSION_BUTTON_MASK
            );

            IOWR_ALTERA_AVALON_PIO_EDGE_CAP(
                ESP_DATA_READY_BASE,
                ESP_DATA_READY_ACTIVE_MASK
            );
        }

        /*
           After session start:
           ESP_DATA_READY rising edge means one packet is ready.
        */
        if (session_started && esp_data_ready_pending)
        {
            esp_data_ready_pending = 0;

            /*
               If signal is already low, ignore this stale edge.
            */
            if (!esp_data_ready_is_high())
            {
                IOWR_ALTERA_AVALON_PIO_EDGE_CAP(
                    ESP_DATA_READY_BASE,
                    ESP_DATA_READY_ACTIVE_MASK
                );

                usleep(1000);
                continue;
            }

            printf("ESP_DATA_READY edge received, reading one SPI packet\n");
            fflush(stdout);

            img_irq_tx_set_false();

            /*
               Your spi.c does:
                   1. send 0xA1 length request
                   2. read packet length
                   3. send 0xA2 packet request
                   4. read packet bytes
                   5. store text/image/status into SDRAM
                   6. update spi_get_latest_text()
            */
            spi_request_text_from_esp();

            packet_len = spi_get_latest_packet_length();

            if (packet_len == 0)
            {
                printf("SPI read failed; waiting for next ESP_DATA_READY edge\n");
                fflush(stdout);

                IOWR_ALTERA_AVALON_PIO_EDGE_CAP(
                    ESP_DATA_READY_BASE,
                    ESP_DATA_READY_ACTIVE_MASK
                );

                usleep(50000);
                continue;
            }

            rx_text = spi_get_latest_text();

            if (rx_text == NULL || rx_text[0] == '\0')
            {
                printf("SPI read OK, packet_len=%lu, text empty\n",
                       (unsigned long)packet_len);
                fflush(stdout);

                img_irq_tx_set_true();

                IOWR_ALTERA_AVALON_PIO_EDGE_CAP(
                    ESP_DATA_READY_BASE,
                    ESP_DATA_READY_ACTIVE_MASK
                );

                usleep(50000);
                continue;
            }

            printf("SPI read OK, packet_len=%lu, text=[%s]\n",
                   (unsigned long)packet_len,
                   rx_text);
            fflush(stdout);

            /*
               Streaming decoder handles split chunks, for example:
                   s akeapp
                   lex5y2x
                   y10x5y18

               It accumulates them into:
                   SNAKEAPPLEX5Y2XY10X5Y18
            */
            decoder_result = decoder_decode_and_store_snake_command(rx_text);

            print_decoder_result(decoder_result);

            /*
               Notify/ACK after handling packet.
            */
            img_irq_tx_set_true();

            IOWR_ALTERA_AVALON_PIO_EDGE_CAP(
                ESP_DATA_READY_BASE,
                ESP_DATA_READY_ACTIVE_MASK
            );

            fflush(stdout);
            usleep(50000);
        }

        usleep(1000);
    }

    return 0;
}
