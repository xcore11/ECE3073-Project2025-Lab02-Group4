#include "system.h"
#include "altera_avalon_pio_regs.h"
#include "sys/alt_irq.h"
#include <stdio.h>
#include <string.h>
#include "control.h"
volatile int switch_state = 0;
volatile int key_state = 0;
volatile int GPIO_state = 0;
static int HEX_enable_bit = 0;
static int scroll_counter = 0;
static int scroll_offset = 0;
const int scroll_speed = 2000;
#define CON_IMG_IRQ_RX_IRQ_INTERRUPT_CONTROLLER_ID 0
#define CON_IMG_IRQ_RX_BASE 0x8011120
#define CON_VGA_IRQ_RX_IRQ_INTERRUPT_CONTROLLER_ID 0
#define CON_VGA_IRQ_RX_BASE 0x8011100
#define CON_IMG_IRQ_TX_IRQ_INTERRUPT_CONTROLLER_ID -1
#define CON_IMG_IRQ_TX_BASE 0x8011130
#define CON_VGA_IRQ_TX_BASE 0x8011110
#define CON_IMG_IRQ_RX_IRQ 3
#define CON_VGA_IRQ_RX_IRQ 4

static void switch_isr(void* context) {
    // Clear the edge capture register
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PIO_SW_BASE, 0xF);

    // Updates the switch switch_state
    switch_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_SW_BASE);
}

void switch_setup(void) {
    // Clear pending switch triggers
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PIO_SW_BASE, 0xF);

    // Enable hardware interrupts
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(PIO_SW_BASE, 0xF);

    // Register the ISR with the processor
    alt_ic_isr_register(
        PIO_SW_IRQ_INTERRUPT_CONTROLLER_ID,
        PIO_SW_IRQ,
        switch_isr,
        NULL,
        NULL
    );

    // Setup initial switch switch_state
    switch_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_SW_BASE);
}

static void key_isr(void* context) {
    // Clear the edge capture register
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PIO_PB_BASE, 0x3);

    // Updates the key key_state
    key_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_PB_BASE);
}

void key_setup(void) {
    // Clear pending key triggers
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PIO_PB_BASE, 0x3);

    // Enable hardware interrupts
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(PIO_PB_BASE, 0x3);

    // Register the ISR with the processor
    alt_ic_isr_register(
        PIO_PB_IRQ_INTERRUPT_CONTROLLER_ID,
        PIO_PB_IRQ,
        key_isr,
        NULL,
        NULL
    );

    // Setup initial key key_state
    key_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_PB_BASE);
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

void handle_key1(void)
{
	if ((~key_state) & 0x01)
	{
		IOWR_ALTERA_AVALON_PIO_DATA(CON_IMG_IRQ_TX_BASE, 0x1);
		IOWR_ALTERA_AVALON_PIO_DATA(CON_IMG_IRQ_TX_BASE, 0x0);
		GPIO_state = GPIO_state | 0x1;
		IOWR_ALTERA_AVALON_PIO_DATA(PIO_GPIO_BASE, GPIO_state);
		key_state = (key_state | 0x01);
	}
}

void handle_key2(void)
{
	if ((~key_state) & 0x02)
	{
		IOWR_ALTERA_AVALON_PIO_DATA(CON_VGA_IRQ_TX_BASE, 0x1);
		IOWR_ALTERA_AVALON_PIO_DATA(CON_VGA_IRQ_TX_BASE, 0x0);
		GPIO_state = GPIO_state | 0x2;
		IOWR_ALTERA_AVALON_PIO_DATA(PIO_GPIO_BASE, GPIO_state);
		key_state = (key_state | 0x02);
	}
}

void HEX_enable(void)
{
    if (switch_state & 0x01) {
        HEX_enable_bit = 1;
    } else {
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, 0xFF);
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX1_BASE, 0xFF);
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX2_BASE, 0xFF);
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX3_BASE, 0xFF);
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX4_BASE, 0xFF);
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX5_BASE, 0xFF);

        HEX_enable_bit = 0;
        scroll_counter = 0;
        scroll_offset = 0;
    }
}

void handle_switch2(const char *message)
{
    if ((switch_state & 0x02) && HEX_enable_bit) {
        char padded_message[64];
        int msg_len;

        if (message == 0) {
            message = "";
        }

        snprintf(padded_message, sizeof(padded_message), "    %s    ", message);
        msg_len = strlen(padded_message);

        scroll_counter++;

        if (scroll_counter >= scroll_speed) {
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

            scroll_counter = 0;
        }
    } else {
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX5_BASE, 0xFF);
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX4_BASE, 0xFF);
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX3_BASE, 0xFF);
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX2_BASE, 0xFF);

        scroll_counter = 0;
        scroll_offset = 0;
    }
}

void handle_switch3(void)
{
    if ((switch_state & 0x04) && HEX_enable_bit) {
        /* placeholder CPU utilization */
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX1_BASE, translator('8'));
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, translator('7'));
    } else {
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX1_BASE, 0xFF);
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, 0xFF);
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
