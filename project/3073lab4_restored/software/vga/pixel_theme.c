#include "pixel_theme.h"
#include <stdio.h>

static void pt_border(int x, int y, int w, int h, vga_color_t c)
{
    vga_draw_rectangle(x, y, w, 1, c);
    vga_draw_rectangle(x, y + h - 1, w, 1, c);
    vga_draw_rectangle(x, y, 1, h, c);
    vga_draw_rectangle(x + w - 1, y, 1, h, c);
}

static void pt_double_border(int x, int y, int w, int h, vga_color_t hi, vga_color_t lo)
{
    pt_border(x, y, w, h, lo);
    pt_border(x + 2, y + 2, w - 4, h - 4, hi);
}

void pt_print_shadow(int x, int y, const char *text, vga_color_t color)
{
    vga_print_software_text(x + 1, y + 1, text, PT_SHADOW);
    vga_print_software_text(x, y, text, color);
}

void pt_draw_shadow_box(int x, int y, int w, int h, vga_color_t fill, vga_color_t edge, vga_color_t shadow)
{
    vga_draw_rectangle(x + 3, y + 3, w, h, shadow);
    vga_draw_rectangle(x, y, w, h, fill);
    vga_draw_rectangle(x, y, w, 2, edge);
    vga_draw_rectangle(x, y, 2, h, edge);
    vga_draw_rectangle(x, y + h - 2, w, 2, PT_INK);
    vga_draw_rectangle(x + w - 2, y, 2, h, PT_INK);
    vga_draw_rectangle(x + 4, y + 4, w - 8, 1, PT_WHITE);
}

void pt_draw_rx_badge(int x, int y, int active, const char *label)
{
    vga_color_t light = active ? PT_GRASS_LIGHT : PT_RED;
    vga_color_t body = active ? PT_GRASS : PT_AMBER;

    vga_draw_rectangle(x, y, 46, 14, PT_INK);
    vga_draw_rectangle(x + 1, y + 1, 44, 12, PT_DARK);
    vga_draw_circle(x + 8, y + 7, 4, body);
    vga_draw_rectangle(x + 6, y + 4, 3, 2, light);
    vga_print_software_text(x + 16, y + 3, label, active ? PT_GRASS_LIGHT : PT_CREAM);
}

static void pt_cloud(int x, int y, int scale)
{
    int s = scale;
    vga_draw_rectangle(x + 2*s, y + 2*s, 14*s, 4*s, PT_CLOUD_SHADOW);
    vga_draw_rectangle(x + 0*s, y + 3*s, 20*s, 4*s, PT_CLOUD);
    vga_draw_rectangle(x + 4*s, y + 1*s, 8*s, 5*s, PT_CLOUD);
    vga_draw_rectangle(x + 11*s, y + 2*s, 8*s, 4*s, PT_CLOUD);
    vga_draw_rectangle(x + 2*s, y + 6*s, 16*s, 1*s, PT_CLOUD_SHADOW);
}

static void pt_pixel_spark(int x, int y, vga_color_t c)
{
    vga_draw_rectangle(x, y + 2, 5, 1, c);
    vga_draw_rectangle(x + 2, y, 1, 5, c);
}

static void pt_draw_rope_platform(int x, int y, int w)
{
    int i;
    vga_draw_rectangle(x + 2, y + 8, w, 6, PT_WOOD_DARK);
    vga_draw_rectangle(x, y + 4, w, 6, PT_WOOD);
    vga_draw_rectangle(x, y + 4, w, 1, PT_WOOD_LIGHT);
    for (i = 0; i < w; i += 16)
    {
        vga_draw_rectangle(x + i, y + 3, 2, 8, PT_WOOD_DARK);
        vga_draw_rectangle(x + i + 6, y + 7, 8, 1, PT_GOLD);
    }
}

void pt_menu_draw_background(void)
{
    int x;
    int y;

    vga_fill_background(PT_SKY);
    vga_draw_rectangle(0, 0, 320, 42, PT_SKY_LIGHT);
    vga_draw_rectangle(0, 42, 320, 44, PT_SKY);
    vga_draw_rectangle(0, 86, 320, 56, VGA_RGB332(2,4,3));
    vga_draw_rectangle(0, 142, 320, 98, PT_WATER_DARK);

    for (y = 150; y < 232; y += 14)
    {
        vga_draw_rectangle(0, y, 320, 2, (y & 16) ? PT_WATER : PT_WATER_LIGHT);
        vga_draw_rectangle((y * 3) % 41, y + 6, 68, 1, PT_FOAM);
        vga_draw_rectangle(180 + ((y * 5) % 29), y + 8, 88, 1, PT_WATER_LIGHT);
    }

    pt_cloud(18, 22, 1);
    pt_cloud(224, 28, 1);
    pt_cloud(124, 66, 1);

    /* Distant island silhouettes. */
    vga_draw_rectangle(0, 120, 320, 22, VGA_RGB332(1,3,2));
    for (x = 0; x < 320; x += 28)
        vga_draw_rectangle(x, 112 + ((x / 28) & 1) * 5, 18, 14, VGA_RGB332(1,4,2));

    pt_draw_rope_platform(0, 202, 88);
    pt_draw_rope_platform(228, 204, 92);

    /* Small hero silhouettes on the platforms. */
    vga_draw_rectangle(41, 190, 8, 12, PT_RED);
    vga_draw_rectangle(39, 202, 12, 4, PT_DARK);
    vga_draw_rectangle(276, 185, 10, 17, PT_MAGIC_PURPLE);
    vga_draw_rectangle(273, 181, 16, 6, PT_CREAM);
    vga_draw_rectangle(278, 178, 6, 4, PT_WHITE);

    for (x = 20; x < 310; x += 52)
        pt_pixel_spark(x, 14 + (x % 19), PT_CREAM);
}

void pt_menu_draw_title(void)
{
    pt_print_shadow(102, 26, "AETHER", PT_CREAM);
    pt_print_shadow(78, 44, "TIDES", PT_WATER_LIGHT);

    vga_draw_rectangle(74, 58, 174, 4, PT_SHADOW);
    vga_draw_rectangle(70, 54, 174, 4, PT_GOLD);
    vga_draw_rectangle(84, 63, 144, 2, PT_CREAM);

    vga_draw_rectangle(64, 38, 8, 8, PT_WATER_LIGHT);
    vga_draw_rectangle(246, 40, 8, 8, PT_MAGIC_PURPLE);
    vga_draw_rectangle(258, 53, 5, 5, PT_GOLD);
}

void pt_menu_draw_option(int y, const char *text, int selected)
{
    if (selected)
    {
        pt_draw_shadow_box(58, y - 7, 204, 23, PT_WOOD_DARK, PT_GOLD, PT_SHADOW);
        vga_draw_rectangle(62, y - 3, 196, 15, PT_WOOD);
        vga_draw_rectangle(70, y + 3, 8, 3, PT_CREAM);
        vga_draw_rectangle(244, y + 3, 8, 3, PT_CREAM);
        pt_print_shadow(92, y, text, PT_WHITE);
    }
    else
    {
        vga_draw_rectangle(80, y - 2, 160, 12, PT_WATER_DARK);
        vga_draw_rectangle(80, y + 10, 160, 1, PT_WATER_LIGHT);
        pt_print_shadow(96, y, text, PT_CREAM);
    }
}

void pt_draw_snake_cell_background(int px, int py, int size, int gx, int gy)
{
    vga_color_t base = ((gx + gy) & 1) ? PT_GRASS : VGA_RGB332(1,4,0);
    vga_draw_rectangle(px, py, size - 1, size - 1, base);
    vga_draw_rectangle(px, py, size - 1, 1, PT_GRASS_LIGHT);
    vga_draw_rectangle(px, py + size - 2, size - 1, 1, PT_GRASS_DARK);

    if (((gx * 5 + gy * 3) & 7) == 0)
        vga_draw_rectangle(px + 2, py + 2, 2, 1, PT_GRASS_LIGHT);
    if (((gx * 7 + gy) & 11) == 0)
        vga_draw_rectangle(px + size - 4, py + size - 3, 2, 1, PT_MOSS);
}

void pt_draw_snake_background(int grid_x0, int grid_y0, int grid_w, int grid_h, int cell_size)
{
    int x;
    int y;

    vga_fill_background(PT_GRASS_DARK);
    vga_draw_rectangle(0, 0, 320, 28, PT_WOOD_DARK);
    vga_draw_rectangle(0, 28, 320, 2, PT_GOLD);
    vga_draw_rectangle(0, 218, 320, 22, PT_WOOD_DARK);
    vga_draw_rectangle(0, 218, 320, 2, PT_GOLD);

    for (y = 0; y < grid_h; y++)
        for (x = 0; x < grid_w; x++)
            pt_draw_snake_cell_background(grid_x0 + x * cell_size, grid_y0 + y * cell_size, cell_size, x, y);

    for (x = 10; x < 310; x += 46)
    {
        vga_draw_rectangle(x, 222, 12, 4, PT_LEAF);
        vga_draw_rectangle(x + 5, 220, 6, 7, PT_GRASS_LIGHT);
    }
}

void pt_draw_snake_frame(int grid_x0, int grid_y0, int grid_w, int grid_h, int cell_size)
{
    int x = grid_x0 - 4;
    int y = grid_y0 - 4;
    int w = grid_w * cell_size + 8;
    int h = grid_h * cell_size + 8;

    vga_draw_rectangle(x + 3, y + 3, w, h, PT_SHADOW);
    pt_double_border(x, y, w, h, PT_GOLD, PT_WOOD_DARK);
    vga_draw_rectangle(x + 2, y + 2, w - 4, 2, PT_WOOD_LIGHT);
    vga_draw_rectangle(x + 2, y + h - 4, w - 4, 2, PT_WOOD);
}

void pt_draw_snake_body(int px, int py, int size)
{
    vga_draw_rectangle(px + 1, py + size - 2, size - 2, 1, PT_SHADOW);
    vga_draw_rectangle(px + 1, py + 1, size - 2, size - 3, PT_SNAKE_DARK);
    vga_draw_rectangle(px + 2, py + 1, size - 4, size - 4, PT_SNAKE);
    vga_draw_rectangle(px + 3, py + 2, size - 5, 2, PT_SNAKE_LIGHT);
    vga_draw_rectangle(px + 1, py + 3, 1, size - 5, PT_GRASS_DARK);
    vga_draw_rectangle(px + size - 2, py + 3, 1, size - 5, PT_GRASS_DARK);
}

void pt_draw_snake_head(int px, int py, int size, int dir)
{
    int ex1 = px + 2;
    int ey1 = py + 2;
    int ex2 = px + size - 4;
    int ey2 = py + 2;

    pt_draw_snake_body(px, py, size);
    vga_draw_rectangle(px + 1, py + 1, size - 2, size - 2, PT_SNAKE);
    vga_draw_rectangle(px + 2, py + 1, size - 4, 2, PT_SNAKE_LIGHT);

    if (dir == 0) { ey1 = py + 2; ey2 = py + 2; }
    else if (dir == 1) { ey1 = py + size - 4; ey2 = py + size - 4; }
    else if (dir == 2) { ex1 = px + 2; ey1 = py + 2; ex2 = px + 2; ey2 = py + size - 4; }
    else { ex1 = px + size - 4; ey1 = py + 2; ex2 = px + size - 4; ey2 = py + size - 4; }

    vga_draw_rectangle(ex1, ey1, 2, 2, PT_WHITE);
    vga_draw_rectangle(ex2, ey2, 2, 2, PT_WHITE);
    vga_draw_rectangle(ex1 + 1, ey1 + 1, 1, 1, PT_BLACK);
    vga_draw_rectangle(ex2 + 1, ey2 + 1, 1, 1, PT_BLACK);

    if (dir == 0)
        vga_draw_rectangle(px + size / 2, py - 1, 1, 3, PT_RED);
    else if (dir == 1)
        vga_draw_rectangle(px + size / 2, py + size - 2, 1, 3, PT_RED);
    else if (dir == 2)
        vga_draw_rectangle(px - 1, py + size / 2, 3, 1, PT_RED);
    else
        vga_draw_rectangle(px + size - 2, py + size / 2, 3, 1, PT_RED);
}

void pt_draw_snake_apple(int px, int py, int size)
{
    vga_draw_rectangle(px + 1, py + size - 2, size - 2, 1, PT_SHADOW);
    vga_draw_rectangle(px + 2, py + 2, size - 4, size - 3, PT_RED_DARK);
    vga_draw_rectangle(px + 1, py + 3, size - 2, size - 4, PT_RED);
    vga_draw_rectangle(px + 2, py + 2, 2, 2, PT_PINK);
    vga_draw_rectangle(px + size / 2, py, 1, 3, PT_WOOD_DARK);
    vga_draw_rectangle(px + size / 2 + 1, py + 1, 3, 2, PT_LEAF);
}

void pt_draw_snake_wall(int px, int py, int size)
{
    vga_draw_rectangle(px + 1, py + size - 2, size - 2, 1, PT_SHADOW);
    vga_draw_rectangle(px + 1, py + 1, size - 2, size - 2, PT_STONE_DARK);
    vga_draw_rectangle(px + 2, py + 1, size - 4, 3, PT_STONE_LIGHT);
    vga_draw_rectangle(px + 2, py + 4, size - 4, size - 5, PT_STONE);
    vga_draw_rectangle(px + 1, py + 3, size - 2, 1, PT_INK);
    vga_draw_rectangle(px + 4, py + 1, 1, size - 2, PT_INK);
    vga_draw_rectangle(px + size - 3, py + 5, 2, 1, PT_MOSS);
}

void pt_draw_snake_portal(int px, int py, int size, int variant)
{
    vga_color_t outer = variant ? PT_MAGIC_PURPLE : PT_MAGIC_BLUE;
    vga_color_t inner = variant ? PT_MAGIC_PINK : PT_WATER_LIGHT;

    vga_draw_rectangle(px + 1, py + size - 2, size - 2, 1, PT_SHADOW);
    vga_draw_circle(px + size / 2, py + size / 2, size / 2 - 1, outer);
    vga_draw_circle(px + size / 2, py + size / 2, size / 2 - 3, PT_INK);
    vga_draw_rectangle(px + 2, py + 2, size - 4, 1, inner);
    vga_draw_rectangle(px + 2, py + size - 3, size - 4, 1, inner);
    vga_draw_rectangle(px + 3, py + 3, 1, size - 6, inner);
    vga_draw_rectangle(px + size - 4, py + 3, 1, size - 6, inner);
    vga_draw_rectangle(px + size / 2, py + 1, 1, size - 2, PT_WHITE);
}

void pt_draw_snake_hud_panel(int score)
{
    char score_text[32];
    vga_draw_rectangle(0, 0, 320, 28, PT_WOOD_DARK);
    vga_draw_rectangle(0, 28, 320, 2, PT_GOLD);
    pt_print_shadow(72, 10, "SERPENT ORCHARD", PT_GRASS_LIGHT);
    snprintf(score_text, sizeof(score_text), "SCORE %d", score);
    pt_print_shadow(218, 10, score_text, PT_GOLD);
}

void pt_draw_snake_lose_screen(int score)
{
    char score_text[32];
    pt_draw_snake_background(32, 40, 32, 22, 8);
    vga_draw_rectangle(0, 0, 320, 240, VGA_RGB332(1,1,1));
    pt_draw_shadow_box(44, 52, 232, 118, PT_WOOD_DARK, PT_RED, PT_SHADOW);
    pt_print_shadow(100, 72, "SNAKE FAINTED", PT_RED);
    snprintf(score_text, sizeof(score_text), "FINAL SCORE %d", score);
    pt_print_shadow(84, 98, score_text, PT_GOLD);
    pt_print_shadow(70, 132, "KEY1 RETRY", PT_GRASS_LIGHT);
    pt_print_shadow(70, 148, "SW9 BACK MENU", PT_WATER_LIGHT);
}

void pt_draw_battle_background(void)
{
    int y;
    int x;

    vga_fill_background(PT_WATER_DARK);
    for (y = 0; y < 240; y += 12)
    {
        vga_draw_rectangle(0, y, 320, 2, (y & 16) ? PT_WATER : VGA_RGB332(0,2,3));
        vga_draw_rectangle((y * 7) % 97, y + 5, 78, 1, PT_WATER_LIGHT);
        vga_draw_rectangle(160 + ((y * 5) % 83), y + 9, 62, 1, PT_FOAM);
    }

    /* Tiny islands and floating debris for an indie-game ocean feel. */
    for (x = 8; x < 180; x += 58)
    {
        vga_draw_rectangle(x, 218, 34, 5, PT_DIRT);
        vga_draw_rectangle(x + 4, 212, 25, 6, PT_GRASS);
        vga_draw_rectangle(x + 13, 206, 4, 8, PT_WOOD_DARK);
        vga_draw_rectangle(x + 8, 204, 14, 4, PT_LEAF);
    }
}

void pt_draw_battle_board_backplate(int x, int y, int w, int h)
{
    pt_draw_shadow_box(x, y, w, h, PT_WOOD_DARK, PT_GOLD, PT_SHADOW);
    vga_draw_rectangle(x + 4, y + 4, w - 8, h - 8, PT_WATER_DARK);
}

void pt_draw_battle_panel(int x, int y, int w, int h)
{
    pt_draw_shadow_box(x, y, w, h, PT_WOOD_DARK, PT_GOLD, PT_SHADOW);
    vga_draw_rectangle(x + 4, y + 4, w - 8, h - 8, PT_INK);
    vga_draw_rectangle(x + 7, y + 7, w - 14, 1, PT_WOOD_LIGHT);
}

void pt_draw_battle_ocean_tile(int px, int py, int size, int miss)
{
    vga_draw_rectangle(px, py, size, size, PT_WATER_DARK);
    vga_draw_rectangle(px + 1, py + 1, size - 2, size - 2, PT_WATER);
    vga_draw_rectangle(px + 2, py + 4, size - 5, 1, PT_WATER_LIGHT);
    vga_draw_rectangle(px + 5, py + 10, size - 7, 1, PT_FOAM);
    vga_draw_rectangle(px + 1, py + size - 2, size - 2, 1, VGA_RGB332(0,1,1));

    if (miss)
    {
        vga_draw_circle(px + size / 2, py + size / 2, 4, PT_FOAM);
        vga_draw_circle(px + size / 2, py + size / 2, 2, PT_WATER);
        vga_draw_rectangle(px + 3, py + 7, size - 6, 1, PT_WHITE);
        vga_draw_rectangle(px + 7, py + 3, 1, size - 6, PT_WHITE);
    }
}

void pt_draw_battle_ship_tile(int px, int py, int size, int revealed, int hit)
{
    if (hit)
    {
        vga_draw_rectangle(px + 1, py + 1, size - 2, size - 2, PT_RED_DARK);
        vga_draw_rectangle(px + 3, py + 3, size - 6, size - 6, PT_ORANGE);
        vga_draw_rectangle(px + 5, py + 5, size - 10, size - 10, PT_GOLD);
        vga_draw_rectangle(px + 2, py + 8, size - 4, 2, PT_WHITE);
        vga_draw_rectangle(px + 8, py + 2, 2, size - 4, PT_WHITE);
        vga_draw_rectangle(px + 11, py + 3, 3, 3, PT_DARK);
        return;
    }

    if (revealed)
    {
        vga_draw_rectangle(px + 2, py + 9, size - 4, 4, PT_WOOD_DARK);
        vga_draw_rectangle(px + 3, py + 7, size - 6, 4, PT_WOOD);
        vga_draw_rectangle(px + 5, py + 5, 2, 6, PT_WOOD_LIGHT);
        vga_draw_rectangle(px + 8, py + 3, 6, 5, PT_CREAM);
        vga_draw_rectangle(px + 8, py + 7, 6, 1, PT_SHADOW);
        vga_draw_rectangle(px + 3, py + 12, size - 6, 1, PT_GOLD);
    }
}

void pt_draw_battle_cursor(int px, int py, int size)
{
    vga_draw_rectangle(px - 1, py - 1, size + 2, 2, PT_GOLD);
    vga_draw_rectangle(px - 1, py + size - 1, size + 2, 2, PT_GOLD);
    vga_draw_rectangle(px - 1, py - 1, 2, size + 2, PT_GOLD);
    vga_draw_rectangle(px + size - 1, py - 1, 2, size + 2, PT_GOLD);
    vga_draw_rectangle(px + 3, py + 3, size - 6, 1, PT_WHITE);
    vga_draw_rectangle(px + 3, py + size - 4, size - 6, 1, PT_WHITE);
    vga_draw_rectangle(px + 3, py + 3, 1, size - 6, PT_WHITE);
    vga_draw_rectangle(px + size - 4, py + 3, 1, size - 6, PT_WHITE);
}

void pt_draw_battle_blast(int px, int py, int radius, int kind)
{
    vga_draw_circle(px, py, radius, PT_GOLD);
    vga_draw_circle(px, py, radius / 2 + 1, PT_ORANGE);
    if (kind >= 1)
    {
        vga_draw_rectangle(px - radius, py, radius * 2 + 1, 1, PT_RED);
        vga_draw_rectangle(px, py - radius, 1, radius * 2 + 1, PT_RED);
    }
    if (kind >= 2)
    {
        vga_draw_rectangle(px - radius, py - radius, 3, 3, PT_WHITE);
        vga_draw_rectangle(px + radius - 2, py - radius, 3, 3, PT_WHITE);
        vga_draw_rectangle(px - radius, py + radius - 2, 3, 3, PT_WHITE);
        vga_draw_rectangle(px + radius - 2, py + radius - 2, 3, 3, PT_WHITE);
    }
}

void pt_draw_battle_bomb_icon(int x, int y, int kind, int selected)
{
    vga_color_t edge = selected ? PT_GOLD : PT_WATER_LIGHT;
    pt_draw_shadow_box(x, y, 31, 22, selected ? PT_WOOD : PT_DARK, edge, PT_SHADOW);

    if (kind == 0)
    {
        /* Cannonball / standard shot icon. */
        vga_draw_circle(x + 15, y + 11, 5, PT_STONE_DARK);
        vga_draw_rectangle(x + 13, y + 8, 3, 2, PT_STONE_LIGHT);
        vga_draw_rectangle(x + 20, y + 5, 6, 2, PT_GOLD);
    }
    else if (kind == 1)
    {
        /* Cross-wave shot icon. */
        vga_draw_rectangle(x + 14, y + 5, 3, 12, PT_MAGIC_BLUE);
        vga_draw_rectangle(x + 9, y + 10, 13, 3, PT_MAGIC_BLUE);
        vga_draw_rectangle(x + 15, y + 6, 1, 10, PT_WHITE);
    }
    else
    {
        /* Square storm icon. */
        vga_draw_rectangle(x + 9, y + 6, 13, 11, PT_MAGIC_PURPLE);
        vga_draw_rectangle(x + 11, y + 8, 9, 7, PT_MAGIC_PINK);
        vga_draw_rectangle(x + 13, y + 10, 5, 3, PT_WHITE);
    }
}

void pt_draw_battle_win_popup(void)
{
    pt_draw_shadow_box(42, 72, 236, 92, PT_WOOD_DARK, PT_GOLD, PT_SHADOW);
    vga_draw_rectangle(48, 78, 224, 80, PT_INK);
    pt_print_shadow(82, 88, "FLEET DOWN", PT_GOLD);
    pt_print_shadow(64, 112, "SW8 CONTINUE", PT_GRASS_LIGHT);
    pt_print_shadow(64, 130, "KEYS LOCKED", PT_WATER_LIGHT);
    pt_print_shadow(64, 148, "SW9 MENU", PT_WHITE);
}

void pt_draw_canvas_background(void)
{
    int x;
    int y;
    vga_fill_background(PT_INK);
    vga_draw_rectangle(0, 0, 320, 28, PT_WOOD_DARK);
    vga_draw_rectangle(0, 28, 320, 2, PT_GOLD);
    vga_draw_rectangle(0, 218, 320, 22, PT_WOOD_DARK);
    vga_draw_rectangle(0, 218, 320, 2, PT_GOLD);
    for (x = 12; x < 318; x += 34)
        vga_draw_rectangle(x, 226, 18, 3, (x & 4) ? PT_MAGIC_PURPLE : PT_WATER_LIGHT);
    for (y = 36; y < 210; y += 20)
        vga_draw_rectangle(8, y, 6, 6, (y & 16) ? PT_GOLD : PT_MAGIC_BLUE);
}

void pt_draw_canvas_frame(int x, int y, int w, int h)
{
    pt_draw_shadow_box(x - 4, y - 4, w + 8, h + 8, PT_WOOD_DARK, PT_GOLD, PT_SHADOW);
    vga_draw_rectangle(x - 1, y - 1, w + 2, h + 2, PT_BLACK);
}

void pt_draw_canvas_cell(int px, int py, int size, vga_color_t color)
{
    vga_draw_rectangle(px, py, size, size, color);
    if (size >= 2)
    {
        /* very subtle pixel separation so 96x96 art still looks tiled */
        if (color != PT_BLACK)
            vga_draw_rectangle(px, py, size, 1, (vga_color_t)(color | 0x01));
    }
}
