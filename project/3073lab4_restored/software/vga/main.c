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

#ifndef SCREEN_EMERGENCY
#define SCREEN_EMERGENCY 5
#endif

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
#define FLAG_EMERGENCY_STOP            0x90
#define FLAG_SFX_CLICK                 0xC5C
#define FLAG_SFX_ENTER_SNAKE           0xC60
#define FLAG_SFX_ENTER_DRAW            0xC64
#define FLAG_SFX_ENTER_BATTLE          0xC68

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

/*
   Emergency stop tilt limits.
   Menu navigation uses a small Y threshold (80). Emergency stop intentionally
   uses a much larger threshold plus hysteresis so normal menu/game tilting
   does not trigger it accidentally.

   The confirm count requires several consecutive unsafe readings before the
   caution screen is latched. This prevents one random accelerometer spike from
   immediately registering as an emergency.
*/
#define EMERGENCY_TILT_THRESHOLD        240
#define EMERGENCY_TILT_CONFIRM_COUNT     4
#define EMERGENCY_RECOVER_THRESHOLD      90
#define EMERGENCY_REDRAW_DELAY_US    250000
#define EMERGENCY_KEY_EXIT_MASK      CONTROL_KEY1_MASK

static volatile int vga_irq_pending = 0;
static volatile uint32_t vga_irq_key_pressed_mask = 0;
static volatile uint32_t vga_irq_switch_state = 0;
static volatile uint32_t vga_irq_control_event_type = CONTROL_EVENT_NONE;

static int current_screen = SCREEN_MENU;
static int selected_menu_index = 0;
static int emergency_stop_active = 0;
static int emergency_tilt_confirm_count = 0;

static void shared_write_u32(uint32_t offset, uint32_t value)
{
    IOWR_32DIRECT(SHARED_FLAGS_BASE, offset, value);
}

static uint32_t shared_read_u32(uint32_t offset)
{
    return IORD_32DIRECT(SHARED_FLAGS_BASE, offset);
}

static void enter_screen(int next_screen);

static void trigger_sfx_flag(uint32_t offset)
{
    shared_write_u32(offset, shared_read_u32(offset) + 1);
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
    shared_write_u32(FLAG_EMERGENCY_STOP, 0);
    shared_write_u32(FLAG_IMAGE_READY, 0);
    shared_write_u32(FLAG_TEXT_READY_SHARED, 0);
    shared_write_u32(FLAG_VGA_DISPLAY_DONE, 0);
    shared_write_u32(FLAG_SFX_CLICK, 0);
    shared_write_u32(FLAG_SFX_ENTER_SNAKE, 0);
    shared_write_u32(FLAG_SFX_ENTER_DRAW, 0);
    shared_write_u32(FLAG_SFX_ENTER_BATTLE, 0);
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

static int abs_alt32(alt_32 value)
{
    return (value < 0) ? (int)(-value) : (int)value;
}

static int read_accel_xyz(alt_32 *x, alt_32 *y, alt_32 *z)
{
    if (x == NULL || y == NULL || z == NULL)
        return -1;

    *x = 0;
    *y = 0;
    *z = 0;

    if (accel_read_x(x) != 0 || accel_read_y(y) != 0 || accel_read_z(z) != 0)
        return -1;

    return 0;
}

static int emergency_orientation_is_unsafe(alt_32 x, alt_32 y)
{
    return (abs_alt32(x) >= EMERGENCY_TILT_THRESHOLD ||
            abs_alt32(y) >= EMERGENCY_TILT_THRESHOLD);
}

static int emergency_orientation_is_safe(alt_32 x, alt_32 y)
{
    return (abs_alt32(x) <= EMERGENCY_RECOVER_THRESHOLD &&
            abs_alt32(y) <= EMERGENCY_RECOVER_THRESHOLD);
}

static void draw_emergency_stop_screen(alt_32 x, alt_32 y, alt_32 z)
{
    char buf[48];

    vga_fill_background(COL_BLACK);

    vga_draw_rectangle(0, 0, 320, 240, COL_RED);
    vga_draw_rectangle(6, 6, 308, 228, COL_BLACK);
    vga_draw_rectangle(12, 12, 296, 216, COL_YELLOW);
    vga_draw_rectangle(18, 18, 284, 204, COL_BLACK);

    vga_print_software_text(74, 38, "!!! CAUTION !!!", COL_RED);
    vga_print_software_text(48, 66, "EMERGENCY STOP", COL_YELLOW);
    vga_print_software_text(34, 96, "VGA TILT LIMIT EXCEEDED", COL_WHITE);
    vga_print_software_text(22, 124, "EMERGENCY IS LATCHED", COL_CYAN);
    vga_print_software_text(28, 142, "PRESS KEY1 FOR MENU", COL_CYAN);

    snprintf(buf, sizeof(buf), "X=%ld Y=%ld Z=%ld", (long)x, (long)y, (long)z);
    vga_print_software_text(58, 178, buf, COL_WHITE);
}

static void enter_emergency_stop_screen(alt_32 x, alt_32 y, alt_32 z)
{
    emergency_stop_active = 1;
    emergency_tilt_confirm_count = 0;
    current_screen = SCREEN_EMERGENCY;

    shared_write_u32(FLAG_EMERGENCY_STOP, 1);
    shared_write_u32(FLAG_CURRENT_GAME, GAME_MODE_MENU);
    shared_write_u32(FLAG_CURRENT_MENU, MENU_SELECT_NONE);
    shared_write_u32(FLAG_GAME_RUNNING, 0);
    shared_write_u32(FLAG_DEBUG_MODE, 0);
    shared_write_u32(FLAG_VGA_DISPLAY_DONE, 0);
    shared_write_u32(FLAG_PANEL_MODE_SEQ, shared_read_u32(FLAG_PANEL_MODE_SEQ) + 1);

    draw_emergency_stop_screen(x, y, z);
    shared_write_u32(FLAG_VGA_DISPLAY_DONE, 1);

    printf("[EMERGENCY] tilt stop entered x=%ld y=%ld z=%ld\n", (long)x, (long)y, (long)z);
    fflush(stdout);
}

static int service_emergency_stop(void)
{
    alt_32 x = 0, y = 0, z = 0;

    /*
       Latched emergency mode must be visually stable.  Once the caution
       screen has been drawn by enter_emergency_stop_screen(), do not keep
       reading accelerometer values and do not redraw the VGA every loop.

       The only thing that should happen while latched is handled later in
       handle_irq_button_action(): KEY1 acknowledges the emergency and returns
       to the main menu.
    */
    if (emergency_stop_active || current_screen == SCREEN_EMERGENCY)
        return 1;

    if (read_accel_xyz(&x, &y, &z) != 0)
        return 0;

    if (emergency_orientation_is_unsafe(x, y))
    {
        emergency_tilt_confirm_count++;

        if (emergency_tilt_confirm_count >= EMERGENCY_TILT_CONFIRM_COUNT)
        {
            enter_emergency_stop_screen(x, y, z);
            return 1;
        }

        return 0;
    }

    emergency_tilt_confirm_count = 0;
    return 0;
}

/*
   Public emergency poll used by long-running game update loops.
   main.c normally checks emergency before calling each screen update, but
   Snake has its own blocking step delay. Calling this from snake.c lets the
   caution screen latch during that delay instead of waiting until snake_update()
   fully returns.
*/
int vga_poll_emergency_stop_from_game(void)
{
    return service_emergency_stop();
}

static void update_menu_navigation(void)
{
    int direction = read_menu_direction();
    int previous_menu_index;

    if (direction == 0)
        return;

    previous_menu_index = selected_menu_index;

    if (direction > 0)
    {
        selected_menu_index++;
        if (selected_menu_index >= MENU_ITEM_COUNT)
            selected_menu_index = 0;
    }
    else
    {
        selected_menu_index--;
        if (selected_menu_index < 0)
            selected_menu_index = MENU_ITEM_COUNT - 1;
    }

    trigger_sfx_flag(FLAG_SFX_CLICK);
    shared_write_u32(FLAG_CURRENT_MENU, (uint32_t)selected_menu_index);

    /*
       Dynamic menu switching: only repaint the old and new game option rows.
       The old code called menu_draw() here, which repainted the full background,
       title and platforms on every tilt step. On VGA this looks like the whole
       screen is refreshing/blinking.
    */
    shared_write_u32(FLAG_VGA_DISPLAY_DONE, 0);
    menu_update_selection(previous_menu_index, selected_menu_index);
    shared_write_u32(FLAG_VGA_DISPLAY_DONE, 1);

    usleep(MENU_MOVE_DELAY_US);
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
        trigger_sfx_flag(FLAG_SFX_ENTER_BATTLE);
        ship_game_init();
    }
    else if (current_screen == SCREEN_SNAKE)
    {
        shared_write_u32(FLAG_CURRENT_GAME, GAME_MODE_SNAKE);
        shared_write_u32(FLAG_CURRENT_MENU, MENU_SELECT_NONE);
        shared_write_u32(FLAG_GAME_RUNNING, 1);
        shared_write_u32(FLAG_DEBUG_MODE, 0);
        trigger_sfx_flag(FLAG_SFX_ENTER_SNAKE);
        snake_init();
    }
    else if (current_screen == SCREEN_DRAW)
    {
        shared_write_u32(FLAG_CURRENT_GAME, GAME_MODE_DRAW);
        shared_write_u32(FLAG_CURRENT_MENU, MENU_SELECT_NONE);
        shared_write_u32(FLAG_GAME_RUNNING, 1);
        shared_write_u32(FLAG_DEBUG_MODE, 0);
        trigger_sfx_flag(FLAG_SFX_ENTER_DRAW);
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

    if (emergency_stop_active || current_screen == SCREEN_EMERGENCY)
    {
        /*
           Emergency screen is latched.  Ignore everything except KEY1.
           KEY1 acknowledges the emergency and returns to the main menu.
        */
        if (key_mask & EMERGENCY_KEY_EXIT_MASK)
        {
            printf("[EMERGENCY] KEY1 acknowledged, returning to main menu\n");
            fflush(stdout);

            emergency_stop_active = 0;
            emergency_tilt_confirm_count = 0;
            shared_write_u32(FLAG_EMERGENCY_STOP, 0);
            selected_menu_index = 0;
            enter_screen(SCREEN_MENU);
            usleep(SCREEN_EXIT_DELAY_US);
        }

        wait_until_irq_signal_low();
        return;
    }

    if (event_type == CONTROL_EVENT_SWITCH && key_mask == 0)
    {
        printf("VGA control switch update: SW=0x%03lX\n", (unsigned long)vga_irq_switch_state);
        fflush(stdout);

        if ((vga_irq_switch_state & CONTROL_SW9_MASK) != 0 && current_screen != SCREEN_MENU)
        {
            if ((current_screen == SCREEN_SNAKE && snake_is_lost()) ||
                (current_screen == SCREEN_BATTLE && ship_game_fleet_popup_visible()))
            {
                trigger_sfx_flag(FLAG_SFX_CLICK);
            }
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
                trigger_sfx_flag(FLAG_SFX_CLICK);
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
    if (service_emergency_stop())
    {
        /*
           Emergency stop owns the whole VGA screen.  Do not call any normal
           screen update function and do not redraw the caution page.

           Still service the IRQ flag so KEY1 can acknowledge the latched
           emergency.  handle_irq_button_action() ignores every other input
           while emergency_stop_active is set.
        */
        if (vga_irq_pending)
        {
            vga_irq_pending = 0;
            handle_irq_button_action();
        }
        return;
    }

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
