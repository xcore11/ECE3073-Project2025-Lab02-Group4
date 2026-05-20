#include "menu.h"
#include "vga.h"

/* =========================
   Retro colors
   ========================= */

#define COL_BLACK   0x0
#define COL_DARK    0x1
#define COL_BLUE    0x9
#define COL_GREEN   0xA
#define COL_CYAN    0xB
#define COL_RED     0xC
#define COL_PURPLE  0xD
#define COL_YELLOW  0xE
#define COL_WHITE   0xF

static void draw_retro_border(void)
{
    int i;

    vga_draw_rectangle(0, 0, 320, 4, COL_CYAN);
    vga_draw_rectangle(0, 236, 320, 4, COL_CYAN);
    vga_draw_rectangle(0, 0, 4, 240, COL_CYAN);
    vga_draw_rectangle(316, 0, 4, 240, COL_CYAN);

    vga_draw_rectangle(8, 8, 304, 2, COL_PURPLE);
    vga_draw_rectangle(8, 230, 304, 2, COL_PURPLE);
    vga_draw_rectangle(8, 8, 2, 224, COL_PURPLE);
    vga_draw_rectangle(310, 8, 2, 224, COL_PURPLE);

    for (i = 0; i < 3; i++)
    {
        vga_draw_rectangle(16 + i * 8, 16, 4, 4, COL_YELLOW);
        vga_draw_rectangle(280 + i * 8, 16, 4, 4, COL_YELLOW);
        vga_draw_rectangle(16 + i * 8, 220, 4, 4, COL_YELLOW);
        vga_draw_rectangle(280 + i * 8, 220, 4, 4, COL_YELLOW);
    }
}

static void draw_scanline_effect(void)
{
    int y;

    for (y = 32; y < 220; y += 16)
    {
        vga_draw_rectangle(20, y, 280, 1, COL_DARK);
    }
}

static void draw_title(void)
{
    vga_print_software_text(58, 28, "FPGA RETRO", COL_YELLOW);
    vga_print_software_text(56, 44, "GAME CONSOLE", COL_GREEN);

    vga_draw_rectangle(48, 62, 224, 3, COL_CYAN);
    vga_draw_rectangle(64, 67, 192, 2, COL_PURPLE);
}

static void draw_menu_option(int y, const char *text, int selected)
{
    if (selected)
    {
        vga_draw_rectangle(28, y - 6, 264, 24, COL_BLUE);
        vga_draw_rectangle(34, y - 2, 252, 16, COL_BLACK);

        vga_print_software_text(48, y, ">", COL_YELLOW);
        vga_print_software_text(72, y, text, COL_WHITE);
        vga_print_software_text(258, y, "<", COL_YELLOW);
    }
    else
    {
        vga_print_software_text(72, y, text, COL_GREEN);
    }
}

void menu_init(void)
{
    menu_draw(0);
}

void menu_draw(int selected_index)
{
    vga_fill_background(COL_BLACK);

    draw_retro_border();
    draw_scanline_effect();
    draw_title();

    draw_menu_option(92,  "ULTIMATE SNAKE GAME", selected_index == 0);
    draw_menu_option(122, "REACTION TIME GAME",  selected_index == 1);
    draw_menu_option(152, "DEBUG MENU",          selected_index == 2);

    vga_print_software_text(36, 198, "TILT UP DOWN TO SELECT", COL_CYAN);
    vga_print_software_text(64, 214, "IRQ BUTTON TO ENTER", COL_YELLOW);
}

ScreenState menu_get_selected_screen(int selected_index)
{
    if (selected_index == 0)
    {
        return SCREEN_SNAKE;
    }
    else if (selected_index == 1)
    {
        return SCREEN_REACTION;
    }
    else
    {
        return SCREEN_DEBUG;
    }
}
