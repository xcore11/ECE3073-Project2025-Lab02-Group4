#ifndef VGA_BATTLE_SHIP_H
#define VGA_BATTLE_SHIP_H

#include <stdint.h>

void ship_game_init(void);
void ship_game_update(void);
void ship_game_handle_control_event(uint32_t key_mask,
                                    uint32_t switch_state,
                                    uint32_t event_type);
int ship_game_fleet_popup_visible(void);

#endif
