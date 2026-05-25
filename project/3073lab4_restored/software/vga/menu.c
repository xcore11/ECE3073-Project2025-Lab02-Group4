#include "menu.h"
#include "vga.h"
#include "pixel_theme.h"

/*
   Aether Tides Arcade main menu.

   This replaces the old rectangle-only neon menu with a more indie/pixel-art
   title screen: sky, water, platforms, shaded option boxes and a unified game
   theme. It still uses only the existing VGA drawing API and keeps the same
   menu selection behavior.
*/

void menu_init(void)
{
    menu_draw(0);
}

void menu_draw(int selected_index)
{
    pt_menu_draw_background();
    pt_menu_draw_title();

    pt_menu_draw_option(90,  "SEA RAIDERS",      selected_index == 0);
    pt_menu_draw_option(118, "SERPENT ORCHARD",  selected_index == 1);
    pt_menu_draw_option(146, "PIXEL STUDIO",     selected_index == 2);
    pt_menu_draw_option(174, "DEBUG DOCK",       selected_index == 3);

    pt_print_shadow(36, 214, "TILT UP DOWN  KEY1 ENTER", PT_CREAM);
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
