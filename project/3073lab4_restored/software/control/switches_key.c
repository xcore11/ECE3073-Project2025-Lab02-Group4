#include "system.h"
#include "io.h"
#include <stdint.h>
#include "altera_avalon_pio_regs.h"
#include "sys/alt_irq.h"
#include <stdio.h>
#include <string.h>
#include "control.h"
#include "includes.h"

volatile int switch_state = 0;
volatile int key_state = 0;
volatile int GPIO_state = 0;
static int HEX_enable_bit = 0;
static int scroll_offset = 0;

static uint32_t control_event_seq = 0;
static uint32_t control_switch_event_seq = 0;

static void shared_write32(uint32_t offset, uint32_t value)
{
    IOWR_32DIRECT(CONTROL_START_FLAGS_BASE, offset, value);
}

uint32_t control_get_switch_state(void)
{
    return (uint32_t)(switch_state & CONTROL_SW_MASK);
}

uint32_t control_get_key_state(void)
{
    return (uint32_t)(key_state & CONTROL_KEY_MASK);
}

void control_shared_flags_init(void)
{
    switch_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_SW_BASE) & CONTROL_SW_MASK;
    key_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_PB_BASE) & CONTROL_KEY_MASK;

    control_event_seq = 0;
    control_switch_event_seq = 0;

    shared_write32(FLAG_CONTROL_PROCESSOR_READY, 1);
    shared_write32(FLAG_CONTROL_EVENT_SEQ, control_event_seq);
    shared_write32(FLAG_CONTROL_KEY_STATE, (uint32_t)key_state);
    shared_write32(FLAG_CONTROL_KEY_PRESSED_MASK, 0);
    shared_write32(FLAG_CONTROL_SWITCH_STATE, (uint32_t)switch_state);
    shared_write32(FLAG_CONTROL_SWITCH_EVENT_SEQ, control_switch_event_seq);
    shared_write32(FLAG_CONTROL_LAST_EVENT_TYPE, CONTROL_EVENT_NONE);
    shared_write32(FLAG_CONTROL_LAST_EVENT_VALUE, 0);
}

static void publish_control_event(uint32_t event_type, uint32_t event_value, uint32_t key_pressed_mask)
{
    control_event_seq++;

    shared_write32(FLAG_CONTROL_KEY_STATE, (uint32_t)(key_state & CONTROL_KEY_MASK));
    shared_write32(FLAG_CONTROL_KEY_PRESSED_MASK, key_pressed_mask & CONTROL_KEY_MASK);
    shared_write32(FLAG_CONTROL_SWITCH_STATE, (uint32_t)(switch_state & CONTROL_SW_MASK));
    shared_write32(FLAG_CONTROL_LAST_EVENT_TYPE, event_type);
    shared_write32(FLAG_CONTROL_LAST_EVENT_VALUE, event_value);

    if (event_type == CONTROL_EVENT_SWITCH) {
        control_switch_event_seq++;
        shared_write32(FLAG_CONTROL_SWITCH_EVENT_SEQ, control_switch_event_seq);
    }

    shared_write32(FLAG_CONTROL_EVENT_SEQ, control_event_seq);
}

static void notify_img_and_vga_processors(void)
{
    /* Pulse both interrupt lines. The shared flags above tell IMG/VGA
       whether this was KEY0, KEY1, or a switch update. */
    IOWR_ALTERA_AVALON_PIO_DATA(CON_IMG_IRQ_TX_BASE, 0x1);
    IOWR_ALTERA_AVALON_PIO_DATA(CON_VGA_IRQ_TX_BASE, 0x1);
    IOWR_ALTERA_AVALON_PIO_DATA(CON_IMG_IRQ_TX_BASE, 0x0);
    IOWR_ALTERA_AVALON_PIO_DATA(CON_VGA_IRQ_TX_BASE, 0x0);

    GPIO_state = GPIO_state | 0x3;
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_GPIO_BASE, GPIO_state);
}

static void switch_isr(void* context) {
    // Clear the edge capture register
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PIO_SW_BASE, CONTROL_SW_MASK);

    // Updates the switch switch_state and publishes SW0..SW9 to shared SDRAM.
    switch_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_SW_BASE) & CONTROL_SW_MASK;

    // Informs task that input is updated
    OSSemPost(input_update_sem);
}

void input_task (void* pdata) {
    INT8U err;
    while (1)
    {
        // To put the task on pend unless updated by ISR
        OSSemPend(input_update_sem, 0, &err);

        // Switch event
        publish_control_event(CONTROL_EVENT_SWITCH, (uint32_t)switch_state, 0);

        // Key events
        if ((~key_state) & CONTROL_KEY0_MASK) {
            publish_control_event(CONTROL_EVENT_KEY, CONTROL_KEY0_MASK, CONTROL_KEY0_MASK);
            key_state = (key_state | CONTROL_KEY0_MASK) & CONTROL_KEY_MASK;
        }

        if ((~key_state) & CONTROL_KEY1_MASK) {
            publish_control_event(CONTROL_EVENT_KEY, CONTROL_KEY1_MASK, CONTROL_KEY1_MASK);
            key_state = (key_state | CONTROL_KEY1_MASK) & CONTROL_KEY_MASK;
        }

        // Notify other processors
        notify_img_and_vga_processors();
    }
}

void switch_setup(void) {
    // Clear pending switch triggers
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PIO_SW_BASE, CONTROL_SW_MASK);

    // Enable hardware interrupts
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(PIO_SW_BASE, CONTROL_SW_MASK);

    // Register the ISR with the processor
    alt_ic_isr_register(
        PIO_SW_IRQ_INTERRUPT_CONTROLLER_ID,
        PIO_SW_IRQ,
        switch_isr,
        NULL,
        NULL
    );

    // Setup initial switch switch_state
    switch_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_SW_BASE) & CONTROL_SW_MASK;
}

static void key_isr(void* context) {
    // Clear the edge capture register
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PIO_PB_BASE, CONTROL_KEY_MASK);

    // Updates the key key_state
    key_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_PB_BASE) & CONTROL_KEY_MASK;

    // Informs task that the input is updated
    OSSemPost(input_update_sem);
}

void key_setup(void) {
    // Clear pending key triggers
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PIO_PB_BASE, CONTROL_KEY_MASK);

    // Enable hardware interrupts
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(PIO_PB_BASE, CONTROL_KEY_MASK);

    // Register the ISR with the processor
    alt_ic_isr_register(
        PIO_PB_IRQ_INTERRUPT_CONTROLLER_ID,
        PIO_PB_IRQ,
        key_isr,
        NULL,
        NULL
    );

    // Setup initial key key_state
    key_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_PB_BASE) & CONTROL_KEY_MASK;
}

static void img_rx_isr(void* context) {
    // Clear the edge capture register
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(CON_IMG_IRQ_RX_BASE, 0x1);

    // Resets the GPIO [0]
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_GPIO_BASE, (GPIO_state & 0x2));
    GPIO_state = GPIO_state & 0x2;
}

void img_rx_setup(void) {
    // Clear GPIO triggers
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(CON_IMG_IRQ_RX_BASE, 0x1);

    // Enable hardware interrupts
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(CON_IMG_IRQ_RX_BASE, 0x1);

    // Register the ISR with the processor
    alt_ic_isr_register(
        CON_IMG_IRQ_RX_IRQ_INTERRUPT_CONTROLLER_ID,
        CON_IMG_IRQ_RX_IRQ,
        img_rx_isr,
        NULL,
        NULL
    );
}

static void vga_rx_isr(void* context) {
    // Clear the edge capture register
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(CON_VGA_IRQ_RX_BASE, 0x1);

    // Resets the GPIO [1]
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_GPIO_BASE, (GPIO_state & 0x1));
    GPIO_state = GPIO_state & 0x1;
}

void vga_rx_setup(void) {
    // Clear GPIO triggers
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(CON_VGA_IRQ_RX_BASE, 0x1);

    // Enable hardware interrupts
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(CON_VGA_IRQ_RX_BASE, 0x1);

    // Register the ISR with the processor
    alt_ic_isr_register(
        CON_VGA_IRQ_RX_IRQ_INTERRUPT_CONTROLLER_ID,
        CON_VGA_IRQ_RX_IRQ,
        vga_rx_isr,
        NULL,
        NULL
    );
}

void HEX_task(void* pdata) {
    char padded_message[64];
    int msg_len = 0;

    while(1) {

        // Switch 1
        if (switch_state & 0x01) {
            HEX_enable_bit = 1;
        } else {
            HEX_enable_bit = 0;
            // Turn off displays
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, 0xFF);
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX1_BASE, 0xFF);
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX2_BASE, 0xFF);
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX3_BASE, 0xFF);
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX4_BASE, 0xFF);
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX5_BASE, 0xFF);
        }

        // Switch 2
        if (HEX_enable_bit && (switch_state & 0x02)) {
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX1_BASE, 0xFF);
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, 0xFF);

            // pending
            const char* message = "test123";
            if (message == 0) message = "";

            snprintf(padded_message, sizeof(padded_message), "    %s    ", message);
            msg_len = strlen(padded_message);

            char c5 = padded_message[scroll_offset];
            char c4 = padded_message[scroll_offset + 1];
            char c3 = padded_message[scroll_offset + 2];
            char c2 = padded_message[scroll_offset + 3];

            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX5_BASE, translator(c5));
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX4_BASE, translator(c4));
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX3_BASE, translator(c3));
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX2_BASE, translator(c2));

            scroll_offset++;
            if (scroll_offset > (msg_len - 4)) {
                scroll_offset = 0;
            }

            // (period of 200 ms = frequency of 5 Hz)
            OSTimeDlyHMSM(0, 0, 0, 200);

        }

        // Switch 3
        else if (HEX_enable_bit && (switch_state & 0x04)) {
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX5_BASE, 0xFF);
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX4_BASE, 0xFF);
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX3_BASE, 0xFF);
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX2_BASE, 0xFF);

            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX1_BASE, translator('8'));
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, translator('7'));

            // (period of 500 ms = frequency of 2 Hz)
            OSTimeDlyHMSM(0, 0, 0, 500);
        }

        // Sleep and wait
        else {
            if (HEX_enable_bit) {
                IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX5_BASE, 0xFF);
                IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX4_BASE, 0xFF);
                IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX3_BASE, 0xFF);
                IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX2_BASE, 0xFF);
                IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX1_BASE, 0xFF);
                IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, 0xFF);
            }
            OSTimeDlyHMSM(0, 0, 0, 100);
        }
    }
}

void handle_switch4(void)
{
    if (switch_state & 0x08) {
        play_speaker(1000, 1);
    } else {
        play_speaker(1000, 0);
    }
}

int translator(char a)
{
    if (a >= '0' && a <= '9') {
        const int num_table[10] = {
            0xC0, 0xF9, 0xA4, 0xB0, 0x99,
            0x92, 0x82, 0xF8, 0x80, 0x90
        };
        return num_table[a - '0'];
    }

    if (a >= 'a' && a <= 'z') {
        a = a - 'a' + 'A';
    }

    if (a >= 'A' && a <= 'Z') {
        const int alpha_table[26] = {
            0x88, 0x83, 0xC6, 0xA1, 0x86, 0x8E, 0x90, 0x89, 0xF9, 0xF1,
            0x8A, 0xC7, 0xC8, 0xAB, 0xC0, 0x8C, 0x98, 0xAF, 0x92, 0x87,
            0xC1, 0xE3, 0x81, 0x89, 0x91, 0xA4
        };
        return alpha_table[a - 'A'];
    }

    if (a == ' ') {
        return 0xFF;
    }

    return 0xFF;
}
