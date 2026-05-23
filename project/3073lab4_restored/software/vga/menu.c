#include "menu.h"
#include "vga.h"

/*
   Local RGB332 constants. Do not use COL_* here so this file cannot redefine
   color aliases from vga.h.
*/
#define MENU_COL_BLACK   0x00
#define MENU_COL_DARK    0x49
#define MENU_COL_BLUE    0x03
#define MENU_COL_GREEN   0x1C
#define MENU_COL_CYAN    0x1F
#define MENU_COL_RED     0xE0
#define MENU_COL_PURPLE  0xE3
#define MENU_COL_YELLOW  0xFC
#define MENU_COL_WHITE   0xFF

static void draw_retro_border(void)
{
    int i;

    vga_draw_rectangle(0, 0, 320, 4, MENU_COL_CYAN);
    vga_draw_rectangle(0, 236, 320, 4, MENU_COL_CYAN);
    vga_draw_rectangle(0, 0, 4, 240, MENU_COL_CYAN);
    vga_draw_rectangle(316, 0, 4, 240, MENU_COL_CYAN);

    vga_draw_rectangle(8, 8, 304, 2, MENU_COL_PURPLE);
    vga_draw_rectangle(8, 230, 304, 2, MENU_COL_PURPLE);
    vga_draw_rectangle(8, 8, 2, 224, MENU_COL_PURPLE);
    vga_draw_rectangle(310, 8, 2, 224, MENU_COL_PURPLE);

    for (i = 0; i < 3; i++)
    {
        vga_draw_rectangle(16 + i * 8, 16, 4, 4, MENU_COL_YELLOW);
        vga_draw_rectangle(280 + i * 8, 16, 4, 4, MENU_COL_YELLOW);
        vga_draw_rectangle(16 + i * 8, 220, 4, 4, MENU_COL_YELLOW);
        vga_draw_rectangle(280 + i * 8, 220, 4, 4, MENU_COL_YELLOW);
    }
}

static void draw_scanline_effect(void)
{
    int y;

    for (y = 28; y < 220; y += 14)
        vga_draw_rectangle(20, y, 280, 1, MENU_COL_DARK);
}

static void draw_title(void)
{
    vga_print_software_text(54, 24, "FPGA RETRO", MENU_COL_YELLOW);
    vga_print_software_text(44, 40, "GRAPHIC ARCADE", MENU_COL_GREEN);
    vga_draw_rectangle(46, 58, 228, 3, MENU_COL_CYAN);
    vga_draw_rectangle(64, 64, 192, 2, MENU_COL_PURPLE);
}

static void draw_menu_option(int y, const char *text, int selected)
{
    if (selected)
    {
        vga_draw_rectangle(22, y - 6, 276, 24, MENU_COL_BLUE);
        vga_draw_rectangle(28, y - 2, 264, 16, MENU_COL_BLACK);
        vga_print_software_text(38, y, ">", MENU_COL_YELLOW);
        vga_print_software_text(62, y, text, MENU_COL_WHITE);
        vga_print_software_text(278, y, "<", MENU_COL_YELLOW);
    }
    else
    {
        vga_print_software_text(62, y, text, MENU_COL_GREEN);
    }
}

void menu_init(void)
{
    menu_draw(0);
}

void menu_draw(int selected_index)
{
    vga_fill_background(MENU_COL_BLACK);

    draw_retro_border();
    draw_scanline_effect();
    draw_title();

    draw_menu_option(84,  "BATTLESHIP EX",      selected_index == 0);
    draw_menu_option(110, "ULTIMATE SNAKE",     selected_index == 1);
    draw_menu_option(136, "DRAW PIXEL GAME",    selected_index == 2);
    draw_menu_option(162, "DEBUG MENU",         selected_index == 3);

    vga_print_software_text(40, 198, "TILT UP DOWN TO SELECT", MENU_COL_CYAN);
    vga_print_software_text(62, 214, "KEY1 / IRQ TO ENTER", MENU_COL_YELLOW);
}

int menu_get_selected_screen(int selected_index)
{
    if (selected_index == 0)
        return SCREEN_BATTLE;
    else if (selected_index == 1)
        return SCREEN_SNAKE;
    else if (selected_index == 2)
        return SCREEN_DRAW;
    else
        return SCREEN_DEBUG;
}
