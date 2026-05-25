#include "menu.h"
#include "vga.h"
#include "pixel_theme.h"

/*
   Aether Tides Arcade main menu.

   Important redraw behavior:
   - menu_draw() is a full redraw used only when entering the menu screen.
   - menu_update_selection() is the dynamic path used while tilting up/down.
     It changes the highlighted game without refreshing the full VGA screen.

   This version intentionally keeps the original main screen layout:
   - no extra bottom blue information bar
   - original option positions
   - original static instruction text only
*/

#define MENU_ITEM_COUNT_LOCAL 4

static const int menu_option_y[MENU_ITEM_COUNT_LOCAL] = {90, 118, 146, 174};
static const char *menu_option_label[MENU_ITEM_COUNT_LOCAL] = {
    "SEA RAIDERS",
    "SERPENT ORCHARD",
    "PIXEL STUDIO",
    "DEBUG DOCK"
};

static int menu_index_is_valid(int index)
{
    return (index >= 0 && index < MENU_ITEM_COUNT_LOCAL);
}

static void menu_draw_all_options(int selected_index)
{
    int i;
    for (i = 0; i < MENU_ITEM_COUNT_LOCAL; i++)
        pt_menu_draw_option(menu_option_y[i], menu_option_label[i], selected_index == i);
}

static void menu_draw_static_instruction(void)
{
    /* Keep the old/original main menu footer. No solid blue strip is drawn here. */
    pt_print_shadow(36, 214, "TILT UP DOWN  KEY1 ENTER", PT_CREAM);
}

void menu_init(void)
{
    menu_draw(0);
}

void menu_draw(int selected_index)
{
    if (!menu_index_is_valid(selected_index))
        selected_index = 0;

    pt_menu_draw_background();
    pt_menu_draw_title();
    menu_draw_all_options(selected_index);
    menu_draw_static_instruction();
}

void menu_update_selection(int previous_index, int selected_index)
{
    if (!menu_index_is_valid(selected_index))
        selected_index = 0;

    if (previous_index == selected_index)
        return;

    /*
       Redraw only the two option rows that visually changed.
       Do not redraw the footer and do not paint a bottom status strip.
    */
    if (menu_index_is_valid(previous_index))
        pt_menu_draw_option(menu_option_y[previous_index], menu_option_label[previous_index], 0);

    pt_menu_draw_option(menu_option_y[selected_index], menu_option_label[selected_index], 1);
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
