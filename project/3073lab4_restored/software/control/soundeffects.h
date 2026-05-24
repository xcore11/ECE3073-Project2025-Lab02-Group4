#ifndef SOUNDEFFECTS_H
#define SOUNDEFFECTS_H

// Initializes the speaker hardware (if needed)
void sound_init(void);

// Retro Arcade Sound Effects
void sfx_laser_shoot(void);
void sfx_explosion(void);
void sfx_portal(void);
void sfx_menu_blip(void);
void sfx_error_buzz(void);
void sfx_eat_apple(void);
void sfx_end_screen(void);
void sfx_change(void);
void sfx_click(void);
void sfx_enter_snake(void);
void sfx_enter_draw(void);
void sfx_enter_battle(void);
void sfx_clear(void);
void sfx_snake_turn(void);

#endif // SOUNDEFFECTS_H
