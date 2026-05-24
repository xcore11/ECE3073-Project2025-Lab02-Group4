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

#endif // SOUNDEFFECTS_H
