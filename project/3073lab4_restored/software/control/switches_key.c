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

static uint32_t control_event_seq = 0;
static uint32_t control_switch_event_seq = 0;

static void shared_write32(uint32_t offset, uint32_t value)
{
    IOWR_32DIRECT(SHARED_FLAGS_BASE, offset, value);
}

static uint32_t shared_read32(uint32_t offset)
{
    return IORD_32DIRECT(SHARED_FLAGS_BASE, offset);
}

uint32_t control_get_switch_state(void)
{
    return (uint32_t)(switch_state & CONTROL_SW_MASK);
}

uint32_t control_get_key_state(void)
{
    return (uint32_t)(key_state & CONTROL_KEY_MASK);
}

static void set_all_hex_off(void)
{
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, 0xFF);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX1_BASE, 0xFF);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX2_BASE, 0xFF);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX3_BASE, 0xFF);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX4_BASE, 0xFF);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX5_BASE, 0xFF);
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

static void switch_isr(void *context)
{
    (void)context;

    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PIO_SW_BASE, CONTROL_SW_MASK);
    switch_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_SW_BASE) & CONTROL_SW_MASK;

    if (input_update_sem != NULL) {
        OSSemPost(input_update_sem);
    }
}

void switch_setup(void)
{
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PIO_SW_BASE, CONTROL_SW_MASK);
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(PIO_SW_BASE, CONTROL_SW_MASK);

    alt_ic_isr_register(
        PIO_SW_IRQ_INTERRUPT_CONTROLLER_ID,
        PIO_SW_IRQ,
        switch_isr,
        NULL,
        NULL
    );

    switch_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_SW_BASE) & CONTROL_SW_MASK;
}

static void key_isr(void *context)
{
    (void)context;

    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PIO_PB_BASE, CONTROL_KEY_MASK);
    key_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_PB_BASE) & CONTROL_KEY_MASK;

    if (input_update_sem != NULL) {
        OSSemPost(input_update_sem);
    }
}

void key_setup(void)
{
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PIO_PB_BASE, CONTROL_KEY_MASK);
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(PIO_PB_BASE, CONTROL_KEY_MASK);

    alt_ic_isr_register(
        PIO_PB_IRQ_INTERRUPT_CONTROLLER_ID,
        PIO_PB_IRQ,
        key_isr,
        NULL,
        NULL
    );

    key_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_PB_BASE) & CONTROL_KEY_MASK;
}

static void img_rx_isr(void *context)
{
    (void)context;

    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(CON_IMG_IRQ_RX_BASE, 0x1);

    IOWR_ALTERA_AVALON_PIO_DATA(PIO_GPIO_BASE, (GPIO_state & 0x2));
    GPIO_state = GPIO_state & 0x2;

    if (leds_update_sem != NULL) {
        OSSemPost(leds_update_sem);
    }
}

void img_rx_setup(void)
{
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(CON_IMG_IRQ_RX_BASE, 0x1);
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(CON_IMG_IRQ_RX_BASE, 0x1);

    alt_ic_isr_register(
        CON_IMG_IRQ_RX_IRQ_INTERRUPT_CONTROLLER_ID,
        CON_IMG_IRQ_RX_IRQ,
        img_rx_isr,
        NULL,
        NULL
    );
}

static void vga_rx_isr(void *context)
{
    (void)context;

    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(CON_VGA_IRQ_RX_BASE, 0x1);

    IOWR_ALTERA_AVALON_PIO_DATA(PIO_GPIO_BASE, (GPIO_state & 0x1));
    GPIO_state = GPIO_state & 0x1;

    if (leds_update_sem != NULL) {
        OSSemPost(leds_update_sem);
    }
}

void vga_rx_setup(void)
{
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(CON_VGA_IRQ_RX_BASE, 0x1);
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(CON_VGA_IRQ_RX_BASE, 0x1);

    alt_ic_isr_register(
        CON_VGA_IRQ_RX_IRQ_INTERRUPT_CONTROLLER_ID,
        CON_VGA_IRQ_RX_IRQ,
        vga_rx_isr,
        NULL,
        NULL
    );
}

void input_task(void *pdata)
{
    INT8U err;

    (void)pdata;

    while (1) {
        OSSemPend(input_update_sem, 0, &err);

        /* Refresh both inputs when any edge arrives. */
        switch_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_SW_BASE) & CONTROL_SW_MASK;
        key_state = IORD_ALTERA_AVALON_PIO_DATA(PIO_PB_BASE) & CONTROL_KEY_MASK;

        publish_control_event(CONTROL_EVENT_SWITCH, (uint32_t)switch_state, 0);

        if ((~key_state) & CONTROL_KEY0_MASK) {
            publish_control_event(CONTROL_EVENT_KEY, CONTROL_KEY0_MASK, CONTROL_KEY0_MASK);
            key_state = (key_state | CONTROL_KEY0_MASK) & CONTROL_KEY_MASK;
        }

        if ((~key_state) & CONTROL_KEY1_MASK) {
            publish_control_event(CONTROL_EVENT_KEY, CONTROL_KEY1_MASK, CONTROL_KEY1_MASK);
            key_state = (key_state | CONTROL_KEY1_MASK) & CONTROL_KEY_MASK;
        }

        notify_img_and_vga_processors();
    }
}

void leds_update_task(void *pdata)
{
    (void)pdata;

    while (1) {
        /* Poll the DEBUG mailbox so IMG-decoded LED commands work even if no
           explicit Control interrupt is produced by the IMG processor. */
        debug_control_update();
        OSTimeDlyHMSM(0, 0, 0, 10);
    }
}

void speaker_switch_task(void *pdata)
{
    int i;

    (void)pdata;

    while (1) {
        if (!control_sfx_busy) {
            int sw4_on = ((control_get_switch_state() & CONTROL_SW4_MASK) != 0);

            /* play_speaker() is a software-toggle driver. It must be called
               many times, not only once per OS tick. */
            for (i = 0; i < 600; i++) {
                debug_control_service_speaker(sw4_on);
            }
        }

        OSTimeDlyHMSM(0, 0, 0, 1);
    }
}

void HEX_task(void *pdata)
{
    char sdram_message[DEBUG_CONTROL_MESSAGE_BYTES];
    char padded_message[DEBUG_CONTROL_MESSAGE_BYTES + 16];
    int scroll_offset = 0;

    extern INT8U OSCPUUsage;

    (void)pdata;

    while (1) {
        uint32_t sw = control_get_switch_state();

        /* Switch 1: HEX master enable. OFF means all HEX displays are off. */
        if ((sw & CONTROL_SW1_MASK) == 0) {
            set_all_hex_off();
            scroll_offset = 0;
            OSTimeDlyHMSM(0, 0, 0, 100);
            continue;
        }

        /* Switch 2: display captured/decoded sentence as scrolling message.
           Priority follows your friend's code: SW2 wins over SW3. */
        if ((sw & CONTROL_SW2_MASK) != 0) {
            int msg_len;
            int i;
            char chars[6];

            debug_control_copy_message(sdram_message, sizeof(sdram_message));

            if (sdram_message[0] == '\0') {
                strcpy(sdram_message, "NO MESSAGE");
            }

            snprintf(padded_message, sizeof(padded_message), "      %s      ", sdram_message);
            msg_len = (int)strlen(padded_message);

            if (msg_len < 6) {
                set_all_hex_off();
                scroll_offset = 0;
            } else {
                if (scroll_offset > msg_len - 6) {
                    scroll_offset = 0;
                }

                for (i = 0; i < 6; i++) {
                    chars[i] = padded_message[scroll_offset + i];
                }

                IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX5_BASE, translator(chars[0]));
                IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX4_BASE, translator(chars[1]));
                IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX3_BASE, translator(chars[2]));
                IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX2_BASE, translator(chars[3]));
                IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX1_BASE, translator(chars[4]));
                IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, translator(chars[5]));

                scroll_offset++;
                if (scroll_offset > msg_len - 6) {
                    scroll_offset = 0;
                }
            }

            OSTimeDlyHMSM(0, 0, 0, 200);
        }
        /* Switch 3: display CPU utilization percentage. */
        else if ((sw & CONTROL_SW3_MASK) != 0) {
            int cpu_load = OSCPUUsage;

            scroll_offset = 0;

            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX5_BASE, 0xFF);
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX4_BASE, 0xFF);
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX3_BASE, 0xFF);

            if (cpu_load >= 100) {
                IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX2_BASE, translator('1'));
                IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX1_BASE, translator('0'));
                IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, translator('0'));
            } else {
                char tens = (char)('0' + (cpu_load / 10));
                char ones = (char)('0' + (cpu_load % 10));

                IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX2_BASE, 0xFF);
                IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX1_BASE, translator(tens));
                IOWR_ALTERA_AVALON_PIO_DATA(PIO_HEX0_BASE, translator(ones));
            }

            OSTimeDlyHMSM(0, 0, 0, 500);
        } else {
            set_all_hex_off();
            scroll_offset = 0;
            OSTimeDlyHMSM(0, 0, 0, 100);
        }
    }
}

/* Legacy compatibility wrappers. RTOS tasks now do the real work. */
void handle_key1(void) { }
void handle_key2(void) { }
void HEX_enable(void) { }
void handle_switch2(const char *message) { (void)message; }
void handle_switch3(void) { }
void handle_switch4(void)
{
    if (!control_sfx_busy) {
        debug_control_service_speaker((control_get_switch_state() & CONTROL_SW4_MASK) != 0);
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
        a = (char)(a - 'a' + 'A');
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
