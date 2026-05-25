#ifndef VGA_PROJECT_MENU_SCREEN_IDS_H_INCLUDED
#define VGA_PROJECT_MENU_SCREEN_IDS_H_INCLUDED

/*
   Screen identifiers are macros, not enum values.

   This avoids stale Nios/Eclipse enum/indexer problems:
   - Symbol 'SCREEN_MENU' / 'SCREEN_SNAKE' / etc could not be resolved
   - redefinition of ScreenState / SCREEN_* when old generated files are mixed in
*/
#ifndef SCREEN_MENU
#define SCREEN_MENU   0
#endif

#ifndef SCREEN_BATTLE
#define SCREEN_BATTLE 1
#endif

#ifndef SCREEN_SNAKE
#define SCREEN_SNAKE  2
#endif

#ifndef SCREEN_DRAW
#define SCREEN_DRAW   3
#endif

#ifndef SCREEN_DEBUG
#define SCREEN_DEBUG  4
#endif

void menu_init(void);
void menu_draw(int selected_index);

/*
   Fast menu update path.
   Use this when the menu is already on screen and only the selected game changes.
   It redraws only the old option row, new option row, and the small selected-game
   info strip instead of clearing/repainting the whole title screen.
*/
void menu_update_selection(int previous_index, int selected_index);

int menu_get_selected_screen(int selected_index);

#endif /* VGA_PROJECT_MENU_SCREEN_IDS_H_INCLUDED */
