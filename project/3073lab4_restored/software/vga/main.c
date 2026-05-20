#include "io.h"
#include "system.h"
#include "vga.h"

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>

#include "altera_avalon_pio_regs.h"
#include "sys/alt_irq.h"

#include "menu.h"
#include "snake.h"
#include "time.h"
#include "debug.h"

/* =========================
   VGA IRQ RX defines
   Same as old code
   ========================= */

#define VGA_IRQ_IRQ_RX 1
#define VGA_IRQ_IRQ_RX_INTERRUPT_CONTROLLER_ID 0
#define VGA_IRQ_RX_BASE 0x9060

#define VGA_IRQ_RX_ACTIVE_MASK 0x01

/* =========================
   Shared flags SDRAM page
   ========================= */

#define SHARED_FLAGS_BASE              0x05212000

#define FLAG_SYSTEM_MAGIC              0x00
#define FLAG_SESSION_STARTED           0x04
#define FLAG_IMAGE_PROCESSOR_READY     0x08
#define FLAG_VGA_PROCESSOR_READY       0x0C
#define FLAG_CONTROL_PROCESSOR_READY   0x10
#define FLAG_CURRENT_MENU              0x14
#define FLAG_CURRENT_GAME              0x18
#define FLAG_GAME_RUNNING              0x1C
#define FLAG_DEBUG_MODE                0x20
#define FLAG_IMAGE_READY               0x24
#define FLAG_TEXT_READY_SHARED         0x28
#define FLAG_VGA_DISPLAY_DONE          0x2C
#define FLAG_LAST_COMMAND              0x30
#define FLAG_LAST_ERROR_SHARED         0x34

/*
   New main-menu-specific flags.
   These use unused space inside the same shared flags page.
*/
#define FLAG_MENU_ENTER_EVENT          0x38
#define FLAG_MENU_EXIT_EVENT           0x3C

/* Values written to FLAG_CURRENT_GAME */
#define GAME_MODE_MENU                 0
#define GAME_MODE_SNAKE                1
#define GAME_MODE_REACTION             2
#define GAME_MODE_DEBUG                3

/* Values written to FLAG_CURRENT_MENU */
#define MENU_SELECT_SNAKE              0
#define MENU_SELECT_REACTION           1
#define MENU_SELECT_DEBUG              2
#define MENU_SELECT_NONE               0xFFFFFFFF

/* =========================
   Shared flag helpers
   ========================= */

static void shared_write_u32(uint32_t offset, uint32_t value)
{
    IOWR_32DIRECT(SHARED_FLAGS_BASE, offset, value);
}

static uint32_t shared_read_u32(uint32_t offset)
{
    return IORD_32DIRECT(SHARED_FLAGS_BASE, offset);
}

static void shared_flags_init_for_vga(void)
{
    /*
       Magic value just means the shared flags page is being used.
       ASCII-ish: 'GAME' = 0x47414D45
    */
    shared_write_u32(FLAG_SYSTEM_MAGIC, 0x47414D45);

    /*
       Tell other processors VGA processor is alive.
    */
    shared_write_u32(FLAG_VGA_PROCESSOR_READY, 1);

    /*
       Initial UI state.
    */
    shared_write_u32(FLAG_CURRENT_MENU, MENU_SELECT_SNAKE);
    shared_write_u32(FLAG_CURRENT_GAME, GAME_MODE_MENU);
    shared_write_u32(FLAG_GAME_RUNNING, 0);
    shared_write_u32(FLAG_DEBUG_MODE, 0);

    /*
       Event flags start cleared.
    */
    shared_write_u32(FLAG_MENU_ENTER_EVENT, 0);
    shared_write_u32(FLAG_MENU_EXIT_EVENT, 0);

    /*
       VGA starts as not done until first menu draw completes.
    */
    shared_write_u32(FLAG_VGA_DISPLAY_DONE, 0);
}

/* =========================
   Menu settings
   ========================= */

#define MENU_ITEM_COUNT 3
#define ACCEL_MENU_THRESHOLD 80
#define ACCEL_LEFT_THRESHOLD 80
#define MENU_MOVE_DELAY_US 250000
#define SCREEN_EXIT_DELAY_US 250000

/* =========================
   State
   ========================= */

static volatile int vga_irq_pending = 0;

static ScreenState current_screen = SCREEN_MENU;
static int selected_menu_index = 0;

/* =========================
   VGA RX IRQ ISR
   Same as old code
   ========================= */

static void vga_irq_rx_isr(void* context)
{
    /*
       Clear/acknowledge PIO edge interrupt.
    */
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(VGA_IRQ_RX_BASE, VGA_IRQ_RX_ACTIVE_MASK);

    /*
       Tell main loop to do the slow drawing work.
    */
    vga_irq_pending = 1;
}

/* =========================
   VGA RX IRQ setup
   Same as old code
   ========================= */

static void vga_irq_rx_setup(void)
{
    /*
       Disable first while configuring.
    */
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(VGA_IRQ_RX_BASE, 0x0);

    /*
       Clear pending edge capture.
    */
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(VGA_IRQ_RX_BASE, VGA_IRQ_RX_ACTIVE_MASK);

    /*
       Register ISR using legacy API.
    */
    alt_irq_register(
        VGA_IRQ_IRQ_RX,
        NULL,
        vga_irq_rx_isr
    );

    /*
       Clear again after registering, then enable.
    */
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(VGA_IRQ_RX_BASE, VGA_IRQ_RX_ACTIVE_MASK);
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(VGA_IRQ_RX_BASE, VGA_IRQ_RX_ACTIVE_MASK);
}

/* =========================
   IRQ line wait helper
   ========================= */

static void wait_until_irq_signal_low(void)
{
    while ((IORD_ALTERA_AVALON_PIO_DATA(VGA_IRQ_RX_BASE) & VGA_IRQ_RX_ACTIVE_MASK) != 0)
    {
        usleep(1000);
    }

    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(VGA_IRQ_RX_BASE, VGA_IRQ_RX_ACTIVE_MASK);
}

/* =========================
   Accelerometer helpers
   ========================= */

static int read_menu_direction(void)
{
    alt_32 x = 0;
    alt_32 y = 0;
    alt_32 z = 0;

    if (accel_read_x(&x) != 0 ||
        accel_read_y(&y) != 0 ||
        accel_read_z(&z) != 0)
    {
        return 0;
    }

    if (y > ACCEL_MENU_THRESHOLD)
    {
        return 1;
    }
    else if (y < -ACCEL_MENU_THRESHOLD)
    {
        return -1;
    }

    return 0;
}

static int board_tilted_left(void)
{
    alt_32 x = 0;
    alt_32 y = 0;
    alt_32 z = 0;

    if (accel_read_x(&x) != 0 ||
        accel_read_y(&y) != 0 ||
        accel_read_z(&z) != 0)
    {
        return 0;
    }

    /*
       If left tilt is reversed, change this to:
       return (x > ACCEL_LEFT_THRESHOLD);
    */
    return (x < -ACCEL_LEFT_THRESHOLD);
}

static void update_menu_navigation(void)
{
    int direction = read_menu_direction();

    if (direction > 0)
    {
        selected_menu_index++;

        if (selected_menu_index >= MENU_ITEM_COUNT)
        {
            selected_menu_index = 0;
        }

        shared_write_u32(FLAG_CURRENT_MENU, (uint32_t)selected_menu_index);

        shared_write_u32(FLAG_VGA_DISPLAY_DONE, 0);
        menu_draw(selected_menu_index);
        shared_write_u32(FLAG_VGA_DISPLAY_DONE, 1);

        usleep(MENU_MOVE_DELAY_US);
    }
    else if (direction < 0)
    {
        selected_menu_index--;

        if (selected_menu_index < 0)
        {
            selected_menu_index = MENU_ITEM_COUNT - 1;
        }

        shared_write_u32(FLAG_CURRENT_MENU, (uint32_t)selected_menu_index);

        shared_write_u32(FLAG_VGA_DISPLAY_DONE, 0);
        menu_draw(selected_menu_index);
        shared_write_u32(FLAG_VGA_DISPLAY_DONE, 1);

        usleep(MENU_MOVE_DELAY_US);
    }
}

/* =========================
   Screen switching
   ========================= */

static void enter_screen(ScreenState next_screen)
{
    /*
       Clear VGA done before drawing a new screen.
    */
    shared_write_u32(FLAG_VGA_DISPLAY_DONE, 0);

    current_screen = next_screen;

    if (current_screen == SCREEN_MENU)
    {
        shared_write_u32(FLAG_CURRENT_GAME, GAME_MODE_MENU);
        shared_write_u32(FLAG_CURRENT_MENU, (uint32_t)selected_menu_index);
        shared_write_u32(FLAG_GAME_RUNNING, 0);
        shared_write_u32(FLAG_DEBUG_MODE, 0);

        menu_draw(selected_menu_index);
    }
    else if (current_screen == SCREEN_SNAKE)
    {
        shared_write_u32(FLAG_CURRENT_GAME, GAME_MODE_SNAKE);
        shared_write_u32(FLAG_CURRENT_MENU, MENU_SELECT_NONE);
        shared_write_u32(FLAG_GAME_RUNNING, 1);
        shared_write_u32(FLAG_DEBUG_MODE, 0);

        snake_init();
    }
    else if (current_screen == SCREEN_REACTION)
    {
        shared_write_u32(FLAG_CURRENT_GAME, GAME_MODE_REACTION);
        shared_write_u32(FLAG_CURRENT_MENU, MENU_SELECT_NONE);
        shared_write_u32(FLAG_GAME_RUNNING, 1);
        shared_write_u32(FLAG_DEBUG_MODE, 0);

        time_game_init();
    }
    else if (current_screen == SCREEN_DEBUG)
    {
        shared_write_u32(FLAG_CURRENT_GAME, GAME_MODE_DEBUG);
        shared_write_u32(FLAG_CURRENT_MENU, MENU_SELECT_NONE);
        shared_write_u32(FLAG_GAME_RUNNING, 0);
        shared_write_u32(FLAG_DEBUG_MODE, 1);

        debug_init();
    }

    /*
       New screen draw completed.
    */
    shared_write_u32(FLAG_VGA_DISPLAY_DONE, 1);
}

static void handle_irq_button_action(void)
{
    if (current_screen == SCREEN_MENU)
    {
        /*
           Menu button means ENTER.
           Increment event counter so other processors can detect edge/event.
        */
        shared_write_u32(
            FLAG_MENU_ENTER_EVENT,
            shared_read_u32(FLAG_MENU_ENTER_EVENT) + 1
        );

        enter_screen(menu_get_selected_screen(selected_menu_index));
        wait_until_irq_signal_low();
    }
    else if (current_screen == SCREEN_SNAKE)
    {
        /*
           Stage 1:
           button exits snake placeholder.
        */
        shared_write_u32(
            FLAG_MENU_EXIT_EVENT,
            shared_read_u32(FLAG_MENU_EXIT_EVENT) + 1
        );

        if (snake_handle_button())
            {
                enter_screen(SCREEN_MENU);
            }

            wait_until_irq_signal_low();
    }
    else if (current_screen == SCREEN_REACTION)
    {
        /*
           Reaction game does not exit by button.
           Button is reserved for reaction press later.
        */
        wait_until_irq_signal_low();
    }
    else if (current_screen == SCREEN_DEBUG)
    {
        /*
           In Debug Menu, button does old display behaviour.
           It displays image/text from SDRAM and keeps console prints.
        */
        shared_write_u32(FLAG_VGA_DISPLAY_DONE, 0);
        debug_display_irq_capture_from_sdram();
        shared_write_u32(FLAG_VGA_DISPLAY_DONE, 1);
    }
}

static void update_current_screen(void)
{
    if (current_screen == SCREEN_MENU)
    {
        update_menu_navigation();
    }
    else if (current_screen == SCREEN_SNAKE)
    {
        snake_update();
    }
    else if (current_screen == SCREEN_REACTION)
    {
        time_game_update();

        if (board_tilted_left())
        {
            shared_write_u32(
                FLAG_MENU_EXIT_EVENT,
                shared_read_u32(FLAG_MENU_EXIT_EVENT) + 1
            );

            enter_screen(SCREEN_MENU);
            usleep(SCREEN_EXIT_DELAY_US);
            return;
        }
    }
    else if (current_screen == SCREEN_DEBUG)
    {
        debug_update();

        if (board_tilted_left())
        {
            shared_write_u32(
                FLAG_MENU_EXIT_EVENT,
                shared_read_u32(FLAG_MENU_EXIT_EVENT) + 1
            );

            enter_screen(SCREEN_MENU);
            usleep(SCREEN_EXIT_DELAY_US);
            return;
        }
    }

    if (vga_irq_pending)
    {
        vga_irq_pending = 0;
        handle_irq_button_action();
        usleep(200000);
    }
}

/* =========================
   main
   ========================= */

int main(void)
{
    vga_init();

    printf("VGA display processor ready\n");
    fflush(stdout);

    /*
       Initialise shared flags before showing menu.
    */
    shared_flags_init_for_vga();

    debug_irq_tx_set_false();

    vga_irq_rx_setup();

    if (accel_init() != 0)
    {
        printf("Accelerometer init failed\n");
        fflush(stdout);
    }
    else
    {
        printf("Accelerometer init OK\n");
        fflush(stdout);
    }

    /*
       Draw initial main menu.
    */
    shared_write_u32(FLAG_VGA_DISPLAY_DONE, 0);
    menu_init();
    shared_write_u32(FLAG_VGA_DISPLAY_DONE, 1);

    while (1)
    {
        update_current_screen();
        usleep(20000);
    }

    return 0;
}
