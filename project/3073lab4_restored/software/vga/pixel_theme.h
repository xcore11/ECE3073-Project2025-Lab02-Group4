#ifndef PIXEL_THEME_H
#define PIXEL_THEME_H

#include <stdint.h>
#include "vga.h"

/*
   Pixel Theme Layer
   -----------------
   This file is intentionally hardware-light. It only uses the existing
   VGA rectangle/circle/software-font API, so it works with the current
   RGB332 framebuffer and does not require image assets, file loading, DMA,
   or a new VGA controller.

   Theme: Aether Tides Arcade
       - menu: sky/ocean floating-island indie title screen
       - snake: grassy orchard, stone walls, magic portals, shaded snake
       - battle: pirate-ocean map, ship hull tiles, foamy water, icon HUD
       - draw: small pixel-art studio/canvas frame
*/

#define PT_BLACK        VGA_RGB332(0,0,0)
#define PT_SHADOW       VGA_RGB332(1,1,1)
#define PT_DARK         VGA_RGB332(2,2,1)
#define PT_INK          VGA_RGB332(0,1,1)
#define PT_WHITE        VGA_RGB332(7,7,3)
#define PT_CREAM        VGA_RGB332(7,6,2)
#define PT_GOLD         VGA_RGB332(7,5,0)
#define PT_AMBER        VGA_RGB332(6,3,0)
#define PT_RED          VGA_RGB332(7,0,0)
#define PT_RED_DARK     VGA_RGB332(3,0,0)
#define PT_ORANGE       VGA_RGB332(7,3,0)
#define PT_PINK         VGA_RGB332(7,2,2)
#define PT_SKY          VGA_RGB332(3,5,3)
#define PT_SKY_LIGHT    VGA_RGB332(4,6,3)
#define PT_CLOUD        VGA_RGB332(6,7,3)
#define PT_CLOUD_SHADOW VGA_RGB332(3,4,2)
#define PT_WATER_DARK   VGA_RGB332(0,1,2)
#define PT_WATER        VGA_RGB332(0,3,3)
#define PT_WATER_LIGHT  VGA_RGB332(1,6,3)
#define PT_FOAM         VGA_RGB332(6,7,3)
#define PT_GRASS_DARK   VGA_RGB332(0,3,0)
#define PT_GRASS        VGA_RGB332(1,5,0)
#define PT_GRASS_LIGHT  VGA_RGB332(4,7,1)
#define PT_LEAF         VGA_RGB332(0,6,0)
#define PT_MOSS         VGA_RGB332(2,4,0)
#define PT_DIRT         VGA_RGB332(4,2,0)
#define PT_WOOD_DARK    VGA_RGB332(2,1,0)
#define PT_WOOD         VGA_RGB332(5,2,0)
#define PT_WOOD_LIGHT   VGA_RGB332(7,4,1)
#define PT_STONE_DARK   VGA_RGB332(2,2,1)
#define PT_STONE        VGA_RGB332(4,4,2)
#define PT_STONE_LIGHT  VGA_RGB332(6,6,3)
#define PT_MAGIC_BLUE   VGA_RGB332(0,5,3)
#define PT_MAGIC_PURPLE VGA_RGB332(5,1,3)
#define PT_MAGIC_PINK   VGA_RGB332(7,1,3)
#define PT_SNAKE_DARK   VGA_RGB332(0,3,0)
#define PT_SNAKE        VGA_RGB332(2,6,0)
#define PT_SNAKE_LIGHT  VGA_RGB332(5,7,1)

void pt_print_shadow(int x, int y, const char *text, vga_color_t color);
void pt_draw_shadow_box(int x, int y, int w, int h, vga_color_t fill, vga_color_t edge, vga_color_t shadow);
void pt_draw_rx_badge(int x, int y, int active, const char *label);

void pt_menu_draw_background(void);
void pt_menu_draw_title(void);
void pt_menu_draw_option(int y, const char *text, int selected);

void pt_draw_snake_background(int grid_x0, int grid_y0, int grid_w, int grid_h, int cell_size);
void pt_draw_snake_frame(int grid_x0, int grid_y0, int grid_w, int grid_h, int cell_size);
void pt_draw_snake_cell_background(int px, int py, int size, int gx, int gy);
void pt_draw_snake_body(int px, int py, int size);
void pt_draw_snake_head(int px, int py, int size, int dir);
void pt_draw_snake_apple(int px, int py, int size);
void pt_draw_snake_wall(int px, int py, int size);
void pt_draw_snake_portal(int px, int py, int size, int variant);
void pt_draw_snake_hud_panel(int score);
void pt_draw_snake_lose_screen(int score);

void pt_draw_battle_background(void);
void pt_draw_battle_board_backplate(int x, int y, int w, int h);
void pt_draw_battle_panel(int x, int y, int w, int h);
void pt_draw_battle_ocean_tile(int px, int py, int size, int miss);
void pt_draw_battle_ship_tile(int px, int py, int size, int revealed, int hit);
void pt_draw_battle_cursor(int px, int py, int size);
void pt_draw_battle_blast(int px, int py, int radius, int kind);
void pt_draw_battle_bomb_icon(int x, int y, int kind, int selected);
void pt_draw_battle_win_popup(void);

void pt_draw_canvas_background(void);
void pt_draw_canvas_frame(int x, int y, int w, int h);
void pt_draw_canvas_cell(int px, int py, int size, vga_color_t color);

#endif
