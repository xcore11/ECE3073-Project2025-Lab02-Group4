#ifndef DEBUG_MENU_H
#define DEBUG_MENU_H

void debug_init(void);
void debug_update(void);

/*
   Same old VGA IRQ debug display behaviour.
*/
void debug_display_irq_capture_from_sdram(void);

/*
   Same old VGA IRQ TX helper used at startup.
*/
void debug_irq_tx_set_false(void);

#endif
