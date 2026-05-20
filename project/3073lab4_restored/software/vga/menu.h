#ifndef MENU_H
#define MENU_H

typedef enum
{
    SCREEN_MENU = 0,
    SCREEN_SNAKE,
    SCREEN_REACTION,
    SCREEN_DEBUG
} ScreenState;

void menu_init(void);
void menu_draw(int selected_index);
ScreenState menu_get_selected_screen(int selected_index);

#endif
