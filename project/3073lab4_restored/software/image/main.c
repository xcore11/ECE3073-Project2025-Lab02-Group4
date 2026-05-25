#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include "io.h"
#include "system.h"
#include "altera_avalon_pio_regs.h"
#include "sys/alt_irq.h"

#include "spi.h"
#include "decoder.h"
#include "decoderdebug.h"
#include "shared_memory.h"

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
static volatile int debug_image_capture_pending = 0;
static volatile uint32_t img_irq_key_pressed_mask = 0;
static volatile uint32_t img_irq_switch_state = 0;
static volatile uint32_t img_irq_control_event_type = CONTROL_EVENT_NONE;
static int session_started = 0;

static uint32_t current_panel_mode = GAME_MODE_MENU;
static uint32_t last_sent_panel_mode = 0xFFFFFFFFu;

static uint32_t shared_read_u32(uint32_t offset)
{
    return IORD_32DIRECT(SHARED_FLAGS_BASE, offset);
}

static void shared_write_u32(uint32_t offset, uint32_t value)
{
    IOWR_32DIRECT(SHARED_FLAGS_BASE, offset, value);
}

static void sync_panel_mode_to_esp(int force_send)
{
    uint32_t mode = shared_read_u32(FLAG_CURRENT_GAME);
    uint32_t old_mode = current_panel_mode;

    if (mode != GAME_MODE_SNAKE && mode != GAME_MODE_DRAW && mode != GAME_MODE_DEBUG && mode != GAME_MODE_BATTLE)
        mode = GAME_MODE_MENU;

    current_panel_mode = mode;

    if (mode != old_mode)
    {
        /*
           Very important for re-entering DEBUG: decoderdebug.c keeps a local
           accumulator plus last-published command values. Control clears the
           0x06000000 mailbox when DEBUG exits, so IMG must also reset its
           decoder state when DEBUG is entered again. Otherwise the same second
           DEBUG instruction looks "unchanged" to IMG and is never republished.
        */
        if (mode == GAME_MODE_DEBUG || old_mode == GAME_MODE_DEBUG)
        {
            decoder_debug_reset();
        }

        if (mode != GAME_MODE_DEBUG)
        {
            decoder_reset_stream();
        }
    }

    if (force_send || mode != last_sent_panel_mode)
    {
        spi_send_panel_mode(mode);
        last_sent_panel_mode = mode;
        shared_write_u32(FLAG_PANEL_MODE_SEQ, shared_read_u32(FLAG_PANEL_MODE_SEQ) + 1);
    }
}

/* ============================================================
   DECODER RESULT PRINT
   ============================================================ */

static void print_decoder_result(int result)
{
#if DEBUG_DECODER_PRINT
    if (result == DECODER_OK_SENT)
    {
        printf("Decoder: realtime command sent to SDRAM mailbox\n");
    }
    else if (result == DECODER_OK_WAITING_PORTAL)
    {
        printf("Decoder: portal stored, waiting for other portal\n");
    }
    else if (result == DECODER_OK_STATE_UPDATED)
    {
        printf("Decoder: realtime state updated\n");
    }
    else if (result == DECODER_NO_COMMAND)
    {
        printf("Decoder: no realtime command detected\n");
    }
    else if (result == DECODER_ERR_BAD_FORMAT)
    {
        printf("Decoder error: bad command format -> reset stream\n");
    }
    else if (result == DECODER_ERR_OUT_OF_RANGE)
    {
        printf("Decoder error: coordinate out of range -> reset stream\n");
    }
    else if (result == DECODER_ERR_MAILBOX_BUSY)
    {
        printf("Decoder error: realtime mailbox busy\n");
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
    uint32_t key_mask;

    (void)context;

    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(
        IMG_IRQ_RX_BASE,
        SESSION_BUTTON_MASK
    );

    /* Control processor pulses this same IRQ line for KEY0/KEY1/SW0..SW9.
       Read shared flags immediately so the main loop knows the event cause. */
    key_mask = shared_read_u32(FLAG_CONTROL_KEY_PRESSED_MASK) & (CONTROL_KEY0_MASK | CONTROL_KEY1_MASK);
    img_irq_key_pressed_mask |= key_mask;
    img_irq_switch_state = shared_read_u32(FLAG_CONTROL_SWITCH_STATE) & CONTROL_SW_MASK;
    img_irq_control_event_type = shared_read_u32(FLAG_CONTROL_LAST_EVENT_TYPE);

    if (!session_started)
    {
        /* First KEY0/legacy IMG IRQ starts the ESP session. */
        session_button_pending = 1;
    }
    else if ((key_mask & CONTROL_KEY0_MASK) != 0)
    {
        /* In Debug panel, KEY0 means capture one image packet now.
           The main loop checks current_panel_mode before sending 0xD4. */
        debug_image_capture_pending = 1;
    }
}

static void session_button_setup(void)
{
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(IMG_IRQ_RX_BASE, 0x0);
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(IMG_IRQ_RX_BASE, SESSION_BUTTON_MASK);

#ifdef IMG_IRQ_RX_IRQ
    /*
       Use legacy alt_irq_register because this BSP reports
       undefined reference to alt_ic_isr_register.
    */
    alt_irq_register(
        IMG_IRQ_RX_IRQ,
        NULL,
        session_button_isr
    );

    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(IMG_IRQ_RX_BASE, SESSION_BUTTON_MASK);
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(IMG_IRQ_RX_BASE, SESSION_BUTTON_MASK);
#else
    /*
       No IRQ number exported in system.h. Main loop still polls
       IMG_IRQ_RX_BASE level, so session start still works.
    */
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(IMG_IRQ_RX_BASE, 0x0);
#endif
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

static void esp_data_ready_setup(void)
{
    /*
       Poll-only ESP data-ready setup.

       Do not reference the ESP data-ready interrupt macro at all. Some BSPs do
       not export it, and Eclipse can still flag it even when it is inside
       #ifdef. The main loop already polls ESP_DATA_READY_BASE, so packet
       reading still works.
    */
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(ESP_DATA_READY_BASE, 0x0);
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(ESP_DATA_READY_BASE, ESP_DATA_READY_ACTIVE_MASK);
    esp_data_ready_pending = 0;
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

static void handle_pending_debug_image_capture(void)
{
    if (!debug_image_capture_pending)
        return;

    debug_image_capture_pending = 0;

    if (current_panel_mode != GAME_MODE_DEBUG)
    {
        printf("Debug image capture ignored: current panel mode is %lu\n",
               (unsigned long)current_panel_mode);
        fflush(stdout);
        return;
    }

    shared_write_u32(FLAG_DEBUG_IMAGE_CAPTURE_REQ,
                     shared_read_u32(FLAG_DEBUG_IMAGE_CAPTURE_REQ) + 1);
    shared_write_u32(FLAG_DEBUG_STATUS, DEBUG_STATUS_CAPTURE_REQUESTED);

    printf("KEY0 debug capture request: sending 0xD4 to ESP\n");
    fflush(stdout);

    spi_send_debug_image_capture();

    shared_write_u32(FLAG_DEBUG_IMAGE_CAPTURE_ACK,
                     shared_read_u32(FLAG_DEBUG_IMAGE_CAPTURE_ACK) + 1);
}

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

    if (current_panel_mode == GAME_MODE_DEBUG)
    {
        decoder_result = decoder_debug_decode_text(rx_text,
                                                   (unsigned int)strlen(rx_text));
        if (decoder_result == DEBUG_DECODER_PUBLISHED)
        {
            printf("Debug decoder: control command published to 0x06000000 mailbox\n");
        }
        else
        {
            printf("Debug decoder: no completed debug command yet\n");
        }
        fflush(stdout);
    }
    else
    {
        decoder_result = decoder_decode_and_store_panel_text_batch((int)current_panel_mode,
                                                             rx_text,
                                                             (unsigned int)strlen(rx_text));
        print_decoder_result(decoder_result);

        /*
           If the stream is malformed, reset decoder so the next realtime header can
           recover cleanly instead of staying poisoned forever.
        */
        if (decoder_result < 0 && decoder_result != DECODER_ERR_MAILBOX_BUSY)
        {
            decoder_reset_stream();
        }
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
    printf("SPI edge-safe packet read + panel-aware realtime decoder enabled\n");
    printf("IMG_IRQ_RX = session start button\n");
    printf("ESP_DATA_READY = packet ready trigger\n");
    printf("==========================================\n");
    fflush(stdout);

    spi_init_manual();
    img_irq_tx_set_false();
    decoder_reset_stream();
    decoder_debug_reset();
    shared_write_u32(FLAG_IMAGE_PROCESSOR_READY, 1);

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
            sync_panel_mode_to_esp(1);

            printf("ESP session start command sent. Panel mode synced. Waiting for ESP_DATA_READY.\n");
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
            sync_panel_mode_to_esp(0);
            handle_pending_debug_image_capture();

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
