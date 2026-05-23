#ifndef DEBUG_MENU_H
#define DEBUG_MENU_H

void debug_init(void);
void debug_update(void);

/* Realtime debug helpers used by VGA main.c when control KEY0/KEY1 IRQs arrive. */
void debug_note_image_capture_requested(void);
void debug_accept_current_image_as_draw_background(void);

/* Backward-compatible manual display function. */
void debug_display_irq_capture_from_sdram(void);

/* Same old VGA IRQ TX helper used at startup. */
void debug_irq_tx_set_false(void);

#endif
