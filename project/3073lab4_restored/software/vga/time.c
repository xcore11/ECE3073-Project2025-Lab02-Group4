#include "time.h"
#include "vga.h"

#define COL_BLACK   0x0
#define COL_GREEN   0xA
#define COL_CYAN    0xB
#define COL_RED     0xC
#define COL_PURPLE  0xD
#define COL_YELLOW  0xE
#define COL_WHITE   0xF

static void draw_time_placeholder(void)
{
    vga_fill_background(COL_BLACK);

    vga_draw_rectangle(0, 0, 320, 4, COL_YELLOW);
    vga_draw_rectangle(0, 236, 320, 4, COL_YELLOW);
    vga_draw_rectangle(0, 0, 4, 240, COL_YELLOW);
    vga_draw_rectangle(316, 0, 4, 240, COL_YELLOW);

    vga_print_software_text(44, 40, "REACTION TIME", COL_YELLOW);
    vga_print_software_text(88, 64, "GAME MODE", COL_GREEN);

    vga_draw_circle(90, 112, 18, COL_RED);
    vga_draw_circle(160, 112, 18, COL_YELLOW);
    vga_draw_circle(230, 112, 18, COL_GREEN);

    vga_print_software_text(28, 154, "REACTION GAME COMING SOON", COL_WHITE);
    vga_print_software_text(48, 190, "IRQ BUTTON TO GO BACK", COL_CYAN);
}

void time_game_init(void)
{
    draw_time_placeholder();
}

void time_game_update(void)
{
    /*
       Future:
       - random LED color
       - target color command
       - timer start
       - button reaction capture
       - VGA reaction time result
    */
}
