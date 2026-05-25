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


static unsigned char pt_letter_mask(char ch, int row)
{
    static const unsigned char A[7] = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11};
    static const unsigned char D[7] = {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E};
    static const unsigned char E[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F};
    static const unsigned char H[7] = {0x11,0x11,0x11,0x1F,0x11,0x11,0x11};
    static const unsigned char I[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F};
    static const unsigned char R[7] = {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11};
    static const unsigned char S[7] = {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E};
    static const unsigned char T[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x04};
    const unsigned char *m = 0;

    if (row < 0 || row >= 7)
        return 0;

    switch (ch)
    {
        case 'A': m = A; break;
        case 'D': m = D; break;
        case 'E': m = E; break;
        case 'H': m = H; break;
        case 'I': m = I; break;
        case 'R': m = R; break;
        case 'S': m = S; break;
        case 'T': m = T; break;
        default: return 0;
    }

    return m[row];
}

static int pt_swag_text_width(const char *text, int scale)
{
    int i;
    int w = 0;
    if (text == 0)
        return 0;

    for (i = 0; text[i] != '\0'; i++)
        w += (text[i] == ' ') ? (3 * scale) : (6 * scale);

    if (w > 0)
        w -= scale;
    return w + (3 * scale);
}

static void pt_draw_swag_text_layer(int x, int y, const char *text, int scale,
                                    vga_color_t color, vga_color_t highlight,
                                    int slant)
{
    int i;
    int cx = x;

    if (text == 0)
        return;

    for (i = 0; text[i] != '\0'; i++)
    {
        int row;
        if (text[i] == ' ')
        {
            cx += 3 * scale;
            continue;
        }

        for (row = 0; row < 7; row++)
        {
            int col;
            unsigned char mask = pt_letter_mask(text[i], row);
            int row_slant = slant ? ((row * scale) / 2) : 0;

            for (col = 0; col < 5; col++)
            {
                if (mask & (1u << (4 - col)))
                {
                    int px = cx + col * scale + row_slant;
                    int py = y + row * scale;
                    vga_draw_rectangle(px, py, scale, scale, color);
                    if (highlight != color && scale >= 3)
                    {
                        vga_draw_rectangle(px, py, scale, 1, highlight);
                        vga_draw_rectangle(px, py, 1, scale, highlight);
                    }
                }
            }
        }

        cx += 6 * scale;
    }
}

static void pt_draw_swag_text(int x, int y, const char *text, int scale,
                              vga_color_t fill, vga_color_t highlight)
{
    /* Big arcade/fighting-game pixel title: black drop, hot-pink under-cut, gold/cyan face. */
    pt_draw_swag_text_layer(x + 5, y + 6, text, scale, PT_BLACK, PT_BLACK, 1);
    pt_draw_swag_text_layer(x + 2, y + 4, text, scale, PT_MAGIC_PINK, PT_MAGIC_PINK, 1);
    pt_draw_swag_text_layer(x - 1, y, text, scale, PT_INK, PT_INK, 1);
    pt_draw_swag_text_layer(x + 1, y, text, scale, PT_INK, PT_INK, 1);
    pt_draw_swag_text_layer(x, y - 1, text, scale, PT_INK, PT_INK, 1);
    pt_draw_swag_text_layer(x, y + 1, text, scale, PT_INK, PT_INK, 1);
    pt_draw_swag_text_layer(x, y, text, scale, fill, highlight, 1);
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
    /* Custom chunky title, closer to the fighting-game / indie splash references. */
    pt_draw_swag_text(83, 14, "AETHER", 3, PT_GOLD, PT_CREAM);
    pt_draw_swag_text(78, 44, "TIDES", 4, PT_WATER_LIGHT, PT_WHITE);

    vga_draw_rectangle(70, 78, 184, 4, PT_BLACK);
    vga_draw_rectangle(66, 74, 184, 4, PT_MAGIC_PINK);
    vga_draw_rectangle(82, 72, 154, 3, PT_GOLD);
    vga_draw_rectangle(94, 82, 128, 2, PT_CREAM);

    vga_draw_rectangle(56, 35, 8, 8, PT_WATER_LIGHT);
    vga_draw_rectangle(254, 43, 8, 8, PT_MAGIC_PURPLE);
    vga_draw_rectangle(270, 60, 5, 5, PT_GOLD);
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

static void pt_draw_side_house(int x, int y, int flip)
{
    (void)flip;

    /* Tiny side-cabin sprite for the empty margins around Serpent Orchard. */
    vga_draw_rectangle(x + 2, y + 23, 22, 3, PT_SHADOW);
    vga_draw_rectangle(x + 4, y + 12, 18, 12, PT_WOOD);
    vga_draw_rectangle(x + 4, y + 12, 18, 2, PT_WOOD_LIGHT);
    vga_draw_rectangle(x + 7, y + 17, 4, 7, PT_INK);
    vga_draw_rectangle(x + 14, y + 16, 5, 4, PT_SKY_LIGHT);
    vga_draw_rectangle(x + 14, y + 16, 5, 1, PT_WHITE);

    vga_draw_rectangle(x + 1, y + 10, 22, 3, PT_RED_DARK);
    vga_draw_rectangle(x + 3, y + 7, 18, 3, PT_RED);
    vga_draw_rectangle(x + 6, y + 4, 12, 3, PT_ORANGE);
    vga_draw_rectangle(x + 9, y + 2, 6, 3, PT_GOLD);

    vga_draw_rectangle(x + 2, y + 14, 1, 9, PT_WOOD_DARK);
    vga_draw_rectangle(x + 22, y + 14, 1, 9, PT_WOOD_DARK);
    vga_draw_rectangle(x + 12, y + 13, 1, 11, PT_WOOD_DARK);
}

static void pt_draw_side_props(void)
{
    /* Left village strip. */
    pt_draw_side_house(3, 58, 0);
    vga_draw_rectangle(9, 97, 3, 15, PT_WOOD_DARK);
    vga_draw_rectangle(5, 90, 11, 9, PT_LEAF);
    vga_draw_rectangle(7, 88, 7, 5, PT_GRASS_LIGHT);
    vga_draw_rectangle(18, 128, 7, 5, PT_STONE);
    vga_draw_rectangle(20, 126, 3, 2, PT_STONE_LIGHT);
    vga_draw_rectangle(4, 165, 21, 3, PT_WOOD_DARK);
    vga_draw_rectangle(7, 158, 2, 10, PT_WOOD);
    vga_draw_rectangle(15, 158, 2, 10, PT_WOOD);

    /* Right village strip. */
    pt_draw_side_house(293, 72, 1);
    vga_draw_rectangle(303, 113, 3, 16, PT_WOOD_DARK);
    vga_draw_rectangle(298, 105, 13, 10, PT_LEAF);
    vga_draw_rectangle(301, 102, 8, 5, PT_GRASS_LIGHT);
    vga_draw_rectangle(292, 145, 8, 5, PT_STONE);
    vga_draw_rectangle(294, 143, 3, 2, PT_STONE_LIGHT);
    vga_draw_rectangle(296, 177, 20, 3, PT_WOOD_DARK);
    vga_draw_rectangle(300, 170, 2, 10, PT_WOOD);
    vga_draw_rectangle(309, 170, 2, 10, PT_WOOD);
}

void pt_draw_snake_cell_background(int px, int py, int size, int gx, int gy)
{
    vga_color_t base = ((gx + gy) & 1) ? PT_DIRT : PT_SAND;
    vga_draw_rectangle(px, py, size - 1, size - 1, base);
    vga_draw_rectangle(px, py, size - 1, 1, PT_SAND_LIGHT);
    vga_draw_rectangle(px, py + size - 2, size - 1, 1, PT_DIRT_DARK);

    /* Dirt speckles instead of grass, so the green snake stays readable. */
    if (((gx * 5 + gy * 3) & 7) == 0)
        vga_draw_rectangle(px + 2, py + 2, 2, 1, PT_DIRT_LIGHT);
    if (((gx * 7 + gy) & 11) == 0)
        vga_draw_rectangle(px + size - 4, py + size - 3, 2, 1, PT_DIRT_DARK);
    if (((gx * 11 + gy * 5) & 15) == 0)
        vga_draw_rectangle(px + size / 2, py + 4, 1, 1, PT_STONE_DARK);
}

void pt_draw_snake_background(int grid_x0, int grid_y0, int grid_w, int grid_h, int cell_size)
{
    int x;
    int y;

    vga_fill_background(PT_DIRT_DARK);
    vga_draw_rectangle(0, 0, 320, 28, PT_WOOD_DARK);
    vga_draw_rectangle(0, 28, 320, 2, PT_GOLD);
    vga_draw_rectangle(0, 30, 320, 188, PT_SAND);
    vga_draw_rectangle(0, 218, 320, 22, PT_WOOD_DARK);
    vga_draw_rectangle(0, 218, 320, 2, PT_GOLD);

    /* Side dirt texture outside the arena. */
    for (y = 34; y < 214; y += 16)
    {
        vga_draw_rectangle(4 + (y % 9), y, 12, 1, PT_DIRT_LIGHT);
        vga_draw_rectangle(296 - (y % 7), y + 8, 14, 1, PT_DIRT);
        vga_draw_rectangle(14, y + 6, 3, 2, PT_STONE_DARK);
        vga_draw_rectangle(302, y + 3, 3, 2, PT_STONE_DARK);
    }

    pt_draw_side_props();

    for (y = 0; y < grid_h; y++)
        for (x = 0; x < grid_w; x++)
            pt_draw_snake_cell_background(grid_x0 + x * cell_size, grid_y0 + y * cell_size, cell_size, x, y);

    for (x = 10; x < 310; x += 46)
    {
        vga_draw_rectangle(x, 222, 12, 4, PT_DIRT_LIGHT);
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
    vga_draw_rectangle(px + 1, py + 3, 1, size - 5, PT_DIRT_DARK);
    vga_draw_rectangle(px + size - 2, py + 3, 1, size - 5, PT_DIRT_DARK);
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

void pt_draw_battle_bottom_ship(int x, int y)
{
    /* Decorative pirate ship breaking out of the lower board area. */
    vga_draw_rectangle(x - 7, y + 24, 76, 3, PT_WATER_LIGHT);
    vga_draw_rectangle(x - 3, y + 27, 68, 2, PT_FOAM);

    /* Hull with pixel-art stepped bow/stern. */
    vga_draw_rectangle(x + 4, y + 15, 48, 9, PT_WOOD_DARK);
    vga_draw_rectangle(x + 8, y + 12, 40, 6, PT_WOOD);
    vga_draw_rectangle(x + 12, y + 10, 31, 3, PT_WOOD_LIGHT);
    vga_draw_rectangle(x + 0, y + 17, 8, 5, PT_WOOD_DARK);
    vga_draw_rectangle(x + 48, y + 14, 10, 7, PT_WOOD_DARK);
    vga_draw_rectangle(x + 8, y + 21, 42, 2, PT_GOLD);
    vga_draw_rectangle(x + 15, y + 15, 5, 4, PT_INK);
    vga_draw_rectangle(x + 28, y + 15, 5, 4, PT_INK);
    vga_draw_rectangle(x + 41, y + 15, 5, 4, PT_INK);

    /* Mast, sail and flag. */
    vga_draw_rectangle(x + 30, y + 0, 3, 18, PT_WOOD_LIGHT);
    vga_draw_rectangle(x + 33, y + 3, 18, 2, PT_WOOD_DARK);
    vga_draw_rectangle(x + 35, y + 5, 13, 10, PT_CREAM);
    vga_draw_rectangle(x + 35, y + 5, 13, 1, PT_WHITE);
    vga_draw_rectangle(x + 45, y + 7, 3, 7, PT_CLOUD_SHADOW);
    vga_draw_rectangle(x + 33, y + 1, 9, 4, PT_RED);
    vga_draw_rectangle(x + 39, y + 2, 4, 2, PT_GOLD);

    /* Cannon muzzle peeking at the player. */
    vga_draw_circle(x + 58, y + 18, 5, PT_STONE_DARK);
    vga_draw_circle(x + 58, y + 18, 2, PT_BLACK);
    vga_draw_rectangle(x + 55, y + 15, 3, 2, PT_STONE_LIGHT);
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

static void pt_draw_brush_icon(int x, int y, int flip)
{
    (void)flip;
    vga_draw_rectangle(x + 2, y + 21, 22, 3, PT_SHADOW);
    vga_draw_rectangle(x + 10, y + 5, 5, 16, PT_WOOD_LIGHT);
    vga_draw_rectangle(x + 11, y + 5, 2, 16, PT_CREAM);
    vga_draw_rectangle(x + 8, y + 18, 9, 4, PT_STONE);
    vga_draw_rectangle(x + 6, y + 21, 13, 6, PT_MAGIC_PINK);
    vga_draw_rectangle(x + 8, y + 25, 8, 3, PT_MAGIC_PURPLE);
    vga_draw_rectangle(x + 5, y + 23, 3, 3, PT_WATER_LIGHT);
}

static void pt_draw_bucket_icon(int x, int y)
{
    vga_draw_rectangle(x + 1, y + 22, 24, 3, PT_SHADOW);
    vga_draw_rectangle(x + 6, y + 8, 14, 15, PT_STONE_DARK);
    vga_draw_rectangle(x + 8, y + 10, 12, 12, PT_STONE);
    vga_draw_rectangle(x + 7, y + 8, 14, 3, PT_STONE_LIGHT);
    vga_draw_rectangle(x + 11, y + 12, 8, 7, PT_WATER);
    vga_draw_rectangle(x + 13, y + 12, 6, 2, PT_WATER_LIGHT);
    vga_draw_rectangle(x + 4, y + 6, 18, 2, PT_CREAM);
    vga_draw_rectangle(x + 4, y + 7, 2, 5, PT_CREAM);
    vga_draw_rectangle(x + 20, y + 7, 2, 5, PT_CREAM);
}

static void pt_draw_palette_icon(int x, int y)
{
    vga_draw_circle(x + 13, y + 14, 11, PT_WOOD_LIGHT);
    vga_draw_circle(x + 16, y + 16, 4, PT_INK);
    vga_draw_rectangle(x + 7, y + 8, 4, 4, PT_RED);
    vga_draw_rectangle(x + 14, y + 6, 4, 4, PT_GOLD);
    vga_draw_rectangle(x + 6, y + 16, 4, 4, PT_MAGIC_BLUE);
    vga_draw_rectangle(x + 13, y + 21, 4, 3, PT_GRASS_LIGHT);
}

void pt_draw_canvas_background(void)
{
    int x;
    int y;
    vga_fill_background(PT_INK);
    vga_draw_rectangle(0, 0, 320, 28, PT_WOOD_DARK);
    vga_draw_rectangle(0, 28, 320, 2, PT_GOLD);
    vga_draw_rectangle(0, 214, 320, 26, PT_WOOD_DARK);
    vga_draw_rectangle(0, 214, 320, 2, PT_GOLD);

    /* Pixel-art studio side tables and tools. */
    vga_draw_rectangle(0, 32, 58, 182, VGA_RGB332(0,1,1));
    vga_draw_rectangle(262, 32, 58, 182, VGA_RGB332(0,1,1));
    vga_draw_rectangle(4, 38, 48, 3, PT_GOLD);
    vga_draw_rectangle(268, 38, 46, 3, PT_GOLD);
    pt_draw_brush_icon(16, 54, 0);
    pt_draw_bucket_icon(15, 98);
    pt_draw_palette_icon(14, 146);
    pt_draw_brush_icon(282, 56, 1);
    pt_draw_bucket_icon(280, 103);
    pt_draw_palette_icon(280, 151);

    for (x = 12; x < 318; x += 34)
        vga_draw_rectangle(x, 226, 18, 3, (x & 4) ? PT_MAGIC_PURPLE : PT_WATER_LIGHT);
    for (y = 48; y < 200; y += 22)
    {
        vga_draw_rectangle(8, y, 6, 6, (y & 16) ? PT_GOLD : PT_MAGIC_BLUE);
        vga_draw_rectangle(306, y + 8, 6, 6, (y & 16) ? PT_MAGIC_PINK : PT_WATER_LIGHT);
    }
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
