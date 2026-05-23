#include "io.h"
#include "system.h"
#include "vga.h"

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>

#include "altera_avalon_pio_regs.h"
#include "sys/alt_irq.h"
#include "alt_types.h"

#include "menu.h"
#include "snake.h"
#include "draw.h"
#include "debug.h"
#include "ship.h"


/* accel.c provides these functions; keep prototypes here so this file does not
   depend on a separate accel.h existing in the Nios project. */
extern int accel_init(void);
extern int accel_read_x(alt_32 *x);
extern int accel_read_y(alt_32 *y);
extern int accel_read_z(alt_32 *z);

#define VGA_IRQ_IRQ_RX 1
#define VGA_IRQ_IRQ_RX_INTERRUPT_CONTROLLER_ID 0
#define VGA_IRQ_RX_BASE 0x9060
#define VGA_IRQ_RX_ACTIVE_MASK 0x01

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
#define FLAG_MENU_ENTER_EVENT          0x38
#define FLAG_MENU_EXIT_EVENT           0x3C
#define FLAG_PANEL_MODE_SEQ            0x8C

#define FLAG_CONTROL_EVENT_SEQ          0x800
#define FLAG_CONTROL_KEY_STATE          0x804
#define FLAG_CONTROL_KEY_PRESSED_MASK   0x808
#define FLAG_CONTROL_SWITCH_STATE       0x80C
#define FLAG_CONTROL_SWITCH_EVENT_SEQ   0x810
#define FLAG_CONTROL_LAST_EVENT_TYPE    0x814
#define FLAG_CONTROL_LAST_EVENT_VALUE   0x818

#define CONTROL_EVENT_NONE              0
#define CONTROL_EVENT_KEY               1
#define CONTROL_EVENT_SWITCH            2

#define CONTROL_KEY0_MASK               0x00000001u
#define CONTROL_KEY1_MASK               0x00000002u
#define CONTROL_SW_MASK                 0x000003FFu
#define CONTROL_SW9_MASK                0x00000200u

#define GAME_MODE_MENU                  0
#define GAME_MODE_SNAKE                 1
#define GAME_MODE_DRAW                  2
#define GAME_MODE_DEBUG                 3
#define GAME_MODE_BATTLE                4

#define MENU_SELECT_BATTLE              0
#define MENU_SELECT_SNAKE               1
#define MENU_SELECT_DRAW                2
#define MENU_SELECT_DEBUG               3
#define MENU_SELECT_NONE                0xFFFFFFFFu

#define MENU_ITEM_COUNT 4
#define ACCEL_MENU_THRESHOLD 80
#define MENU_MOVE_DELAY_US 250000
#define SCREEN_EXIT_DELAY_US 250000

static volatile int vga_irq_pending = 0;
static volatile uint32_t vga_irq_key_pressed_mask = 0;
static volatile uint32_t vga_irq_switch_state = 0;
static volatile uint32_t vga_irq_control_event_type = CONTROL_EVENT_NONE;

static int current_screen = SCREEN_MENU;
static int selected_menu_index = 0;

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
    shared_write_u32(FLAG_SYSTEM_MAGIC, 0x47414D45);
    shared_write_u32(FLAG_VGA_PROCESSOR_READY, 1);
    shared_write_u32(FLAG_CURRENT_MENU, MENU_SELECT_BATTLE);
    shared_write_u32(FLAG_CURRENT_GAME, GAME_MODE_MENU);
    shared_write_u32(FLAG_GAME_RUNNING, 0);
    shared_write_u32(FLAG_DEBUG_MODE, 0);
    shared_write_u32(FLAG_MENU_ENTER_EVENT, 0);
    shared_write_u32(FLAG_MENU_EXIT_EVENT, 0);
    shared_write_u32(FLAG_PANEL_MODE_SEQ, 0);
    shared_write_u32(FLAG_IMAGE_READY, 0);
    shared_write_u32(FLAG_TEXT_READY_SHARED, 0);
    shared_write_u32(FLAG_VGA_DISPLAY_DONE, 0);
}

static void vga_irq_rx_isr(void* context)
{
    uint32_t key_mask;
    (void)context;

    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(VGA_IRQ_RX_BASE, VGA_IRQ_RX_ACTIVE_MASK);

    key_mask = shared_read_u32(FLAG_CONTROL_KEY_PRESSED_MASK) &
               (CONTROL_KEY0_MASK | CONTROL_KEY1_MASK);

    vga_irq_key_pressed_mask |= key_mask;
    vga_irq_switch_state = shared_read_u32(FLAG_CONTROL_SWITCH_STATE) & CONTROL_SW_MASK;
    vga_irq_control_event_type = shared_read_u32(FLAG_CONTROL_LAST_EVENT_TYPE);
    vga_irq_pending = 1;
}

static void vga_irq_rx_setup(void)
{
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(VGA_IRQ_RX_BASE, 0x0);
    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(VGA_IRQ_RX_BASE, VGA_IRQ_RX_ACTIVE_MASK);

    alt_irq_register(VGA_IRQ_IRQ_RX, NULL, vga_irq_rx_isr);

    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(VGA_IRQ_RX_BASE, VGA_IRQ_RX_ACTIVE_MASK);
    IOWR_ALTERA_AVALON_PIO_IRQ_MASK(VGA_IRQ_RX_BASE, VGA_IRQ_RX_ACTIVE_MASK);
}

static void wait_until_irq_signal_low(void)
{
    while ((IORD_ALTERA_AVALON_PIO_DATA(VGA_IRQ_RX_BASE) & VGA_IRQ_RX_ACTIVE_MASK) != 0)
        usleep(1000);

    IOWR_ALTERA_AVALON_PIO_EDGE_CAP(VGA_IRQ_RX_BASE, VGA_IRQ_RX_ACTIVE_MASK);
}

static int read_menu_direction(void)
{
    alt_32 x = 0, y = 0, z = 0;
    if (accel_read_x(&x) != 0 || accel_read_y(&y) != 0 || accel_read_z(&z) != 0)
        return 0;

    if (y > ACCEL_MENU_THRESHOLD) return 1;
    if (y < -ACCEL_MENU_THRESHOLD) return -1;
    return 0;
}

static void update_menu_navigation(void)
{
    int direction = read_menu_direction();

    if (direction > 0)
    {
        selected_menu_index++;
        if (selected_menu_index >= MENU_ITEM_COUNT)
            selected_menu_index = 0;

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
            selected_menu_index = MENU_ITEM_COUNT - 1;

        shared_write_u32(FLAG_CURRENT_MENU, (uint32_t)selected_menu_index);
        shared_write_u32(FLAG_VGA_DISPLAY_DONE, 0);
        menu_draw(selected_menu_index);
        shared_write_u32(FLAG_VGA_DISPLAY_DONE, 1);
        usleep(MENU_MOVE_DELAY_US);
    }
}

static void enter_screen(int next_screen)
{
    shared_write_u32(FLAG_VGA_DISPLAY_DONE, 0);

    current_screen = next_screen;
    shared_write_u32(FLAG_PANEL_MODE_SEQ, shared_read_u32(FLAG_PANEL_MODE_SEQ) + 1);

    if (current_screen == SCREEN_MENU)
    {
        shared_write_u32(FLAG_CURRENT_GAME, GAME_MODE_MENU);
        shared_write_u32(FLAG_CURRENT_MENU, (uint32_t)selected_menu_index);
        shared_write_u32(FLAG_GAME_RUNNING, 0);
        shared_write_u32(FLAG_DEBUG_MODE, 0);
        menu_draw(selected_menu_index);
    }
    else if (current_screen == SCREEN_BATTLE)
    {
        shared_write_u32(FLAG_CURRENT_GAME, GAME_MODE_BATTLE);
        shared_write_u32(FLAG_CURRENT_MENU, MENU_SELECT_NONE);
        shared_write_u32(FLAG_GAME_RUNNING, 1);
        shared_write_u32(FLAG_DEBUG_MODE, 0);
        ship_game_init();
    }
    else if (current_screen == SCREEN_SNAKE)
    {
        shared_write_u32(FLAG_CURRENT_GAME, GAME_MODE_SNAKE);
        shared_write_u32(FLAG_CURRENT_MENU, MENU_SELECT_NONE);
        shared_write_u32(FLAG_GAME_RUNNING, 1);
        shared_write_u32(FLAG_DEBUG_MODE, 0);
        snake_init();
    }
    else if (current_screen == SCREEN_DRAW)
    {
        shared_write_u32(FLAG_CURRENT_GAME, GAME_MODE_DRAW);
        shared_write_u32(FLAG_CURRENT_MENU, MENU_SELECT_NONE);
        shared_write_u32(FLAG_GAME_RUNNING, 1);
        shared_write_u32(FLAG_DEBUG_MODE, 0);
        draw_game_init();
    }
    else if (current_screen == SCREEN_DEBUG)
    {
        shared_write_u32(FLAG_CURRENT_GAME, GAME_MODE_DEBUG);
        shared_write_u32(FLAG_CURRENT_MENU, MENU_SELECT_NONE);
        shared_write_u32(FLAG_GAME_RUNNING, 0);
        shared_write_u32(FLAG_DEBUG_MODE, 1);
        debug_init();
    }

    shared_write_u32(FLAG_VGA_DISPLAY_DONE, 1);
}

static uint32_t consume_vga_irq_key_mask(void)
{
    uint32_t key_mask = vga_irq_key_pressed_mask;
    vga_irq_key_pressed_mask = 0;
    return key_mask;
}

static void handle_irq_button_action(void)
{
    uint32_t key_mask = consume_vga_irq_key_mask();
    uint32_t event_type = vga_irq_control_event_type;

    if (event_type == CONTROL_EVENT_SWITCH && key_mask == 0)
    {
        printf("VGA control switch update: SW=0x%03lX\n", (unsigned long)vga_irq_switch_state);
        fflush(stdout);

        if ((vga_irq_switch_state & CONTROL_SW9_MASK) != 0 && current_screen != SCREEN_MENU)
        {
            shared_write_u32(FLAG_MENU_EXIT_EVENT, shared_read_u32(FLAG_MENU_EXIT_EVENT) + 1);
            enter_screen(SCREEN_MENU);
            usleep(SCREEN_EXIT_DELAY_US);
            wait_until_irq_signal_low();
            return;
        }

        if (current_screen == SCREEN_BATTLE)
        {
            ship_game_handle_control_event(0, vga_irq_switch_state, event_type);
        }

        wait_until_irq_signal_low();
        return;
    }

    if (current_screen == SCREEN_MENU)
    {
        if ((key_mask & CONTROL_KEY1_MASK) || (key_mask == 0 && event_type != CONTROL_EVENT_SWITCH))
        {
            shared_write_u32(FLAG_MENU_ENTER_EVENT, shared_read_u32(FLAG_MENU_ENTER_EVENT) + 1);
            enter_screen(menu_get_selected_screen(selected_menu_index));
        }
        wait_until_irq_signal_low();
    }
    else if (current_screen == SCREEN_BATTLE)
    {
        ship_game_handle_control_event(key_mask, vga_irq_switch_state, event_type);
        wait_until_irq_signal_low();
    }
    else if (current_screen == SCREEN_SNAKE)
    {
        /*
           SW9 remains the universal escape handled above.
           KEY1/KEY0 should still work on the YOU LOSE screen, otherwise
           the player gets stuck staring at the retry prompt.
        */
        if (key_mask & (CONTROL_KEY0_MASK | CONTROL_KEY1_MASK))
        {
            if (snake_is_lost())
            {
                snake_handle_button();  /* retry, returns 0 */
            }
            else
            {
                printf("Snake key consumed during live game; use SW9 for menu\n");
                fflush(stdout);
            }
        }
        wait_until_irq_signal_low();
    }
    else if (current_screen == SCREEN_DRAW)
    {
        if (key_mask & CONTROL_KEY1_MASK)
        {
            printf("Draw KEY1 consumed; use SW9 to return to menu\n");
            fflush(stdout);
        }
        wait_until_irq_signal_low();
    }
    else if (current_screen == SCREEN_DEBUG)
    {
        shared_write_u32(FLAG_VGA_DISPLAY_DONE, 0);
        if (key_mask & CONTROL_KEY0_MASK)
            debug_note_image_capture_requested();
        if (key_mask & CONTROL_KEY1_MASK)
            debug_accept_current_image_as_draw_background();
        shared_write_u32(FLAG_VGA_DISPLAY_DONE, 1);
        wait_until_irq_signal_low();
    }
}

static void update_current_screen(void)
{
    if (current_screen == SCREEN_MENU)
        update_menu_navigation();
    else if (current_screen == SCREEN_BATTLE)
        ship_game_update();
    else if (current_screen == SCREEN_SNAKE)
        snake_update();
    else if (current_screen == SCREEN_DRAW)
        draw_game_update();
    else if (current_screen == SCREEN_DEBUG)
        debug_update();

    if (vga_irq_pending)
    {
        vga_irq_pending = 0;
        handle_irq_button_action();
        usleep(200000);
    }
}

int main(void)
{
    vga_init();

    printf("VGA display processor ready\n");
    fflush(stdout);

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
