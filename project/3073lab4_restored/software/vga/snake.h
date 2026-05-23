#ifndef SNAKE_H
#define SNAKE_H

void snake_init(void);
void snake_update(void);
int snake_is_lost(void);

/*
   Returns 1 when the snake screen should exit back to main menu.
   Returns 0 when the button was handled inside snake, for example retry after lose.
*/
int snake_handle_button(void);

#endif
