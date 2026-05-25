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



static unsigned short pt_fantasy_letter_mask(char ch, int row)
{
    /*
       7x9 title alphabet.
       The title renderer below adds outlines, bevels, serifs and leaf/wood accents,
       so the masks stay readable on the 320x240 RGB332 framebuffer.
    */
    static const unsigned short A[9] = {0x1C,0x36,0x63,0x63,0x7F,0x63,0x63,0x63,0x63};
    static const unsigned short C[9] = {0x3E,0x63,0x60,0x60,0x60,0x60,0x60,0x63,0x3E};
    static const unsigned short D[9] = {0x7C,0x66,0x63,0x63,0x63,0x63,0x63,0x66,0x7C};
    static const unsigned short E[9] = {0x7F,0x60,0x60,0x60,0x7C,0x60,0x60,0x60,0x7F};
    static const unsigned short H[9] = {0x63,0x63,0x63,0x63,0x7F,0x63,0x63,0x63,0x63};
    static const unsigned short I[9] = {0x7F,0x1C,0x1C,0x1C,0x1C,0x1C,0x1C,0x1C,0x7F};
    static const unsigned short L[9] = {0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x7F};
    static const unsigned short N[9] = {0x63,0x73,0x7B,0x7F,0x6F,0x67,0x63,0x63,0x63};
    static const unsigned short O[9] = {0x3E,0x63,0x63,0x63,0x63,0x63,0x63,0x63,0x3E};
    static const unsigned short P[9] = {0x7E,0x63,0x63,0x63,0x7E,0x60,0x60,0x60,0x60};
    static const unsigned short R[9] = {0x7E,0x63,0x63,0x63,0x7E,0x6C,0x66,0x63,0x63};
    static const unsigned short S[9] = {0x3F,0x60,0x60,0x60,0x3E,0x03,0x03,0x03,0x7E};
    static const unsigned short T[9] = {0x7F,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08};
    static const unsigned short U[9] = {0x63,0x63,0x63,0x63,0x63,0x63,0x63,0x63,0x3E};
    static const unsigned short X[9] = {0x63,0x63,0x36,0x1C,0x08,0x1C,0x36,0x63,0x63};
    const unsigned short *m = 0;

    if (row < 0 || row >= 9)
        return 0;

    switch (ch)
    {
        case 'A': m = A; break;
        case 'C': m = C; break;
        case 'D': m = D; break;
        case 'E': m = E; break;
        case 'H': m = H; break;
        case 'I': m = I; break;
        case 'L': m = L; break;
        case 'N': m = N; break;
        case 'O': m = O; break;
        case 'P': m = P; break;
        case 'R': m = R; break;
        case 'S': m = S; break;
        case 'T': m = T; break;
        case 'U': m = U; break;
        case 'X': m = X; break;
        default: return 0;
    }
    return m[row];
}

static int pt_fantasy_text_width(const char *text, int scale)
{
    int i;
    int w = 0;
    if (text == 0)
        return 0;

    for (i = 0; text[i] != '\0'; i++)
        w += (text[i] == ' ') ? (4 * scale) : (8 * scale);

    if (w > 0)
        w -= scale;
    return w;
}

static void pt_draw_fantasy_text_layer(int x, int y, const char *text, int scale,
                                       vga_color_t color, vga_color_t highlight,
                                       int serif)
{
    int i;
    int cx = x;
    if (text == 0 || scale <= 0)
        return;

    for (i = 0; text[i] != '\0'; i++)
    {
        int row;
        if (text[i] == ' ')
        {
            cx += 4 * scale;
            continue;
        }

        for (row = 0; row < 9; row++)
        {
            int col;
            unsigned short mask = pt_fantasy_letter_mask(text[i], row);
            for (col = 0; col < 7; col++)
            {
                if (mask & (1u << (6 - col)))
                {
                    int px = cx + col * scale;
                    int py = y + row * scale;
                    vga_draw_rectangle(px, py, scale, scale, color);

                    if (highlight != color && scale >= 2)
                    {
                        vga_draw_rectangle(px, py, scale, 1, highlight);
                        vga_draw_rectangle(px, py, 1, scale, highlight);
                    }
                }
            }
        }

        if (serif && scale >= 2)
        {
            /* Small hand-crafted title-font feet/spikes: makes each character look less plain. */
            vga_draw_rectangle(cx - scale, y, 3 * scale, scale, color);
            vga_draw_rectangle(cx + 5 * scale, y + 8 * scale, 3 * scale, scale, color);
            if (text[i] == 'A' || text[i] == 'R' || text[i] == 'S' || text[i] == 'T')
                vga_draw_rectangle(cx + 6 * scale, y - scale, 2 * scale, scale, color);
            if (text[i] == 'E' || text[i] == 'F')
                vga_draw_rectangle(cx + 6 * scale, y + 4 * scale, 2 * scale, scale, color);
        }

        cx += 8 * scale;
    }
}

static void pt_draw_leaf_pair(int x, int y)
{
    vga_draw_rectangle(x + 2, y + 3, 6, 3, PT_LEAF);
    vga_draw_rectangle(x + 4, y + 2, 3, 5, PT_GRASS_LIGHT);
    vga_draw_rectangle(x + 10, y + 1, 5, 4, PT_LEAF);
    vga_draw_rectangle(x + 10, y + 2, 2, 2, PT_GRASS_LIGHT);
    vga_draw_rectangle(x + 7, y + 5, 6, 1, PT_WOOD_DARK);
}

static void pt_draw_fantasy_title_text(int x, int y, const char *text, int scale,
                                       vga_color_t fill, vga_color_t highlight,
                                       int leaves)
{
    int w = pt_fantasy_text_width(text, scale);

    /* heavy readable outline */
    pt_draw_fantasy_text_layer(x + 4, y + 5, text, scale, PT_BLACK, PT_BLACK, 0);
    pt_draw_fantasy_text_layer(x - scale, y, text, scale, PT_INK, PT_INK, 0);
    pt_draw_fantasy_text_layer(x + scale, y, text, scale, PT_INK, PT_INK, 0);
    pt_draw_fantasy_text_layer(x, y - scale, text, scale, PT_INK, PT_INK, 0);
    pt_draw_fantasy_text_layer(x, y + scale, text, scale, PT_INK, PT_INK, 0);

    /* warm under-shadow, like carved/painted title wood */
    if (scale >= 2)
        pt_draw_fantasy_text_layer(x + scale, y + 2 * scale, text, scale, PT_AMBER, PT_AMBER, 0);

    pt_draw_fantasy_text_layer(x, y, text, scale, fill, highlight, 1);

    if (scale >= 2)
    {
        int by = y + 9 * scale + 2;
        vga_draw_rectangle(x + scale, by, w - 2 * scale, scale, PT_BLACK);
        vga_draw_rectangle(x + 2 * scale, by - scale, w - 4 * scale, scale, highlight);
    }

    if (leaves)
    {
        pt_draw_leaf_pair(x - 13, y + 4 * scale);
        pt_draw_leaf_pair(x + w + 2, y + 2 * scale);
    }
}

static void pt_draw_fantasy_title_centered(int y, const char *text, int scale,
                                           vga_color_t fill, vga_color_t highlight,
                                           int leaves)
{
    int w = pt_fantasy_text_width(text, scale);
    int x = (320 - w) / 2;
    pt_draw_fantasy_title_text(x, y, text, scale, fill, highlight, leaves);
}

static void pt_draw_clean_fantasy_text_centered(int y, const char *text,
                                                vga_color_t fill, vga_color_t highlight)
{
    int w = pt_fantasy_text_width(text, 1);
    int x = (320 - w) / 2;

    /* Clean Pixel Studio title: outline only, no drop shadow and no leaf clutter. */
    pt_draw_fantasy_text_layer(x - 1, y, text, 1, PT_INK, PT_INK, 0);
    pt_draw_fantasy_text_layer(x + 1, y, text, 1, PT_INK, PT_INK, 0);
    pt_draw_fantasy_text_layer(x, y - 1, text, 1, PT_INK, PT_INK, 0);
    pt_draw_fantasy_text_layer(x, y + 1, text, 1, PT_INK, PT_INK, 0);
    pt_draw_fantasy_text_layer(x, y, text, 1, fill, highlight, 1);
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
    /*
       Custom fantasy-pixel wordmark. It avoids a normal software-font look by
       drawing every title character from our own 7x9 masks, then adding bevels,
       carved serifs, shadows, small leaves and a compass underline.
    */
    pt_draw_fantasy_title_centered(12, "AETHER", 3, PT_CREAM, PT_WHITE, 1);
    pt_draw_fantasy_title_centered(44, "TIDES", 4, PT_WATER_LIGHT, PT_WHITE, 1);

    vga_draw_rectangle(92, 85, 136, 3, PT_BLACK);
    vga_draw_rectangle(99, 84, 122, 2, PT_GOLD);
    vga_draw_rectangle(150, 82, 20, 2, PT_CREAM);
    vga_draw_rectangle(158, 75, 3, 17, PT_GOLD);
    vga_draw_rectangle(151, 82, 17, 3, PT_GOLD);
    vga_draw_rectangle(156, 80, 7, 7, PT_WHITE);
    vga_draw_rectangle(158, 82, 3, 3, PT_MAGIC_BLUE);

    vga_draw_rectangle(52, 33, 7, 7, PT_WATER_LIGHT);
    vga_draw_rectangle(259, 40, 7, 7, PT_MAGIC_PURPLE);
    vga_draw_rectangle(275, 62, 5, 5, PT_GOLD);
}

static void pt_menu_clip_rect(int rx, int ry, int rw, int rh,
                              int cx, int cy, int cw, int ch,
                              vga_color_t color)
{
    int x0 = rx;
    int y0 = ry;
    int x1 = rx + rw;
    int y1 = ry + rh;
    int c0 = cx;
    int d0 = cy;
    int c1 = cx + cw;
    int d1 = cy + ch;

    if (x0 < c0) x0 = c0;
    if (y0 < d0) y0 = d0;
    if (x1 > c1) x1 = c1;
    if (y1 > d1) y1 = d1;

    if (x1 > x0 && y1 > y0)
        vga_draw_rectangle(x0, y0, x1 - x0, y1 - y0, color);
}

static void pt_menu_restore_option_row(int y)
{
    /*
       Restore only the menu row area using the real title-screen scenery.
       The rectangle is large enough to erase the selected brown hover button,
       so menu_update_selection() can switch options without a full refresh.
    */
    const int rx = 54;
    const int ry = y - 10;
    const int rw = 212;
    const int rh = 30;
    int yy;
    int wave_y;
    int x;

    for (yy = ry; yy < ry + rh; yy++)
    {
        vga_color_t c;
        if (yy < 42)
            c = PT_SKY_LIGHT;
        else if (yy < 86)
            c = PT_SKY;
        else if (yy < 142)
            c = VGA_RGB332(2,4,3);
        else
            c = PT_WATER_DARK;

        vga_draw_rectangle(rx, yy, rw, 1, c);
    }

    /* Repaint the distant island silhouettes if the restored row crosses them. */
    pt_menu_clip_rect(0, 120, 320, 22, rx, ry, rw, rh, VGA_RGB332(1,3,2));
    for (x = 0; x < 320; x += 28)
    {
        int iy = 112 + ((x / 28) & 1) * 5;
        pt_menu_clip_rect(x, iy, 18, 14, rx, ry, rw, rh, VGA_RGB332(1,4,2));
    }

    /* Repaint only the small water highlights that pass through this row. */
    for (wave_y = 150; wave_y < 232; wave_y += 14)
    {
        pt_menu_clip_rect(0, wave_y, 320, 2, rx, ry, rw, rh,
                          (wave_y & 16) ? PT_WATER : PT_WATER_LIGHT);
        pt_menu_clip_rect((wave_y * 3) % 41, wave_y + 6, 68, 1,
                          rx, ry, rw, rh, PT_FOAM);
        pt_menu_clip_rect(180 + ((wave_y * 5) % 29), wave_y + 8, 88, 1,
                          rx, ry, rw, rh, PT_WATER_LIGHT);
    }
}

void pt_menu_draw_option(int y, const char *text, int selected)
{
    pt_menu_restore_option_row(y);

    if (selected)
    {
        /* Hover/selected state: keep the old brown button, but only for the active row. */
        pt_draw_shadow_box(58, y - 7, 204, 23, PT_WOOD_DARK, PT_GOLD, PT_SHADOW);
        vga_draw_rectangle(62, y - 3, 196, 15, PT_WOOD);
        vga_draw_rectangle(66, y - 1, 188, 1, PT_WOOD_LIGHT);
        vga_draw_rectangle(70, y + 3, 8, 3, PT_CREAM);
        vga_draw_rectangle(244, y + 3, 8, 3, PT_CREAM);
        pt_print_shadow(92, y, text, PT_WHITE);
    }
    else
    {
        /* Idle state: no blue or brown backing bar, only clean text over the scenery. */
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
    /*
       Light natural dirt floor. Keep it close to the snake brightness so the
       snake still reads clearly without the arena becoming high-contrast/noisy.
    */
    unsigned int h = (unsigned int)(gx * 41 + gy * 73 + gx * gy * 17);
    vga_color_t base = PT_SAND_LIGHT;

    if ((h % 19u) < 6u)
        base = PT_SAND;
    if ((h % 29u) == 5u || (h % 31u) == 7u)
        base = PT_DIRT_LIGHT;

    vga_draw_rectangle(px, py, size - 1, size - 1, base);

    /* Per-cell pixel flecks: more texture, but mostly light/subtle shades. */
    if (size >= 8)
    {
        vga_draw_rectangle(px + 1, py + 1 + (int)(h % 2u), 2, 1, PT_SAND);
        vga_draw_rectangle(px + 5, py + 2 + (int)((h >> 2) % 2u), 2, 1, PT_SAND);
        vga_draw_rectangle(px + 2 + (int)((h >> 3) % 3u), py + 5, 2, 1, PT_SAND_LIGHT);
        vga_draw_rectangle(px + 6, py + 6, 1, 1, PT_DIRT_LIGHT);

        if ((h & 3u) == 0u)
            vga_draw_rectangle(px + 1, py + 3, 5, 1, PT_SAND);
        if ((h & 7u) == 5u)
            vga_draw_rectangle(px + 3, py + 6, 4, 1, PT_SAND_LIGHT);
        if ((h % 11u) == 2u)
            vga_draw_rectangle(px + 1, py + size - 3, 3, 1, PT_DIRT_LIGHT);
        if ((h % 17u) == 4u)
            vga_draw_rectangle(px + size - 3, py + 2, 1, 1, PT_STONE);
        if ((h % 23u) == 8u)
            vga_draw_rectangle(px + 2, py + size - 3, 2, 1, PT_MOSS);
    }
    else
    {
        if ((h & 3u) == 0u)
            vga_draw_rectangle(px + 1, py + 1, size - 3, 1, PT_SAND);
    }
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

    /* Ancient forest studio, cleaned up: no top leaf clutter or heavy shadows. */
    vga_fill_background(VGA_RGB332(0,1,1));
    vga_draw_rectangle(0, 0, 320, 38, VGA_RGB332(0,2,1));
    vga_draw_rectangle(0, 38, 320, 176, VGA_RGB332(0,1,1));
    vga_draw_rectangle(0, 38, 320, 2, PT_WOOD_DARK);
    vga_draw_rectangle(0, 40, 320, 1, PT_GOLD);

    /* subtle side forest only; top is kept clean so the title is readable. */
    for (y = 50; y < 206; y += 34)
    {
        vga_draw_rectangle(0, y, 8, 18, PT_GRASS_DARK);
        vga_draw_rectangle(312, y + 6, 8, 18, PT_GRASS_DARK);
        vga_draw_rectangle(4, y + 8, 7, 5, PT_MOSS);
        vga_draw_rectangle(309, y + 12, 7, 5, PT_MOSS);
    }

    /* tree trunks and forest depth */
    vga_draw_rectangle(28, 28, 9, 190, PT_WOOD_DARK);
    vga_draw_rectangle(31, 28, 3, 190, PT_WOOD);
    vga_draw_rectangle(284, 32, 11, 186, PT_WOOD_DARK);
    vga_draw_rectangle(287, 32, 4, 186, PT_WOOD);
    vga_draw_rectangle(0, 214, 320, 26, PT_WOOD_DARK);
    vga_draw_rectangle(0, 214, 320, 2, PT_GOLD);

    /* side tool boards */
    pt_draw_shadow_box(4, 44, 48, 154, PT_WOOD_DARK, PT_GOLD, PT_SHADOW);
    pt_draw_shadow_box(268, 44, 48, 154, PT_WOOD_DARK, PT_GOLD, PT_SHADOW);
    pt_print_shadow(11, 49, "TOOLS", PT_CREAM);
    pt_print_shadow(273, 49, "COLOR", PT_CREAM);

    /* tool slots */
    for (y = 0; y < 3; y++)
    {
        for (x = 0; x < 2; x++)
        {
            vga_draw_rectangle(10 + x * 19, 66 + y * 34, 16, 24, PT_INK);
            vga_draw_rectangle(11 + x * 19, 67 + y * 34, 14, 22, PT_DARK);
        }
    }
    pt_draw_brush_icon(6, 61, 0);
    pt_draw_bucket_icon(25, 61);
    pt_draw_palette_icon(7, 99);
    pt_draw_brush_icon(25, 99, 1);
    pt_draw_bucket_icon(6, 135);
    pt_draw_palette_icon(25, 137);

    /* color swatches and paint table */
    for (y = 0; y < 5; y++)
    {
        for (x = 0; x < 2; x++)
        {
            vga_color_t c = (vga_color_t)((x + y * 2) * 21 + 0x20);
            vga_draw_rectangle(276 + x * 17, 66 + y * 18, 14, 14, PT_INK);
            vga_draw_rectangle(278 + x * 17, 68 + y * 18, 10, 10, c);
            vga_draw_rectangle(278 + x * 17, 68 + y * 18, 10, 1, PT_WHITE);
        }
    }
    vga_draw_rectangle(272, 166, 40, 17, PT_WOOD);
    vga_draw_rectangle(276, 160, 29, 10, PT_WOOD_LIGHT);
    vga_draw_rectangle(280, 163, 5, 4, PT_RED);
    vga_draw_rectangle(288, 162, 5, 4, PT_GOLD);
    vga_draw_rectangle(296, 164, 5, 4, PT_MAGIC_BLUE);

    /* little forest floor details */
    for (x = 58; x < 262; x += 23)
    {
        vga_draw_rectangle(x, 221 + (x & 3), 11, 3, PT_STONE);
        vga_draw_rectangle(x + 3, 218 + (x & 1), 4, 5, PT_GRASS_LIGHT);
    }
    vga_draw_rectangle(58, 222, 8, 5, PT_RED_DARK);
    vga_draw_rectangle(60, 220, 4, 3, PT_CREAM);
    vga_draw_rectangle(250, 222, 8, 5, PT_RED_DARK);
    vga_draw_rectangle(253, 220, 4, 3, PT_CREAM);

    /* same custom title family as the main menu, but clean: no drop shadow/leaves. */
    pt_draw_clean_fantasy_text_centered(7, "PIXEL STUDIO", PT_CREAM, PT_WHITE);
}

void pt_draw_canvas_frame(int x, int y, int w, int h)
{
    int i;
    /* Clean carved wooden frame: no heavy drop shadow and no green leaf clutter. */
    vga_draw_rectangle(x - 7, y - 7, w + 14, h + 14, PT_WOOD_DARK);
    vga_draw_rectangle(x - 5, y - 5, w + 10, h + 10, PT_WOOD);
    vga_draw_rectangle(x - 3, y - 3, w + 6, h + 6, PT_WOOD_LIGHT);
    vga_draw_rectangle(x - 1, y - 1, w + 2, h + 2, PT_BLACK);

    for (i = 0; i < w; i += 18)
    {
        vga_draw_rectangle(x - 5 + i, y - 7, 10, 2, PT_WOOD_LIGHT);
        vga_draw_rectangle(x - 4 + i, y + h + 5, 12, 2, PT_WOOD_DARK);
        vga_draw_rectangle(x - 2 + i, y - 4, 5, 1, PT_CREAM);
        vga_draw_rectangle(x + i, y + h + 3, 7, 1, PT_AMBER);
    }
    for (i = 0; i < h; i += 20)
    {
        vga_draw_rectangle(x - 7, y - 4 + i, 2, 12, PT_WOOD_LIGHT);
        vga_draw_rectangle(x + w + 5, y - 2 + i, 2, 10, PT_WOOD_DARK);
        vga_draw_rectangle(x - 4, y + 2 + i, 1, 6, PT_CREAM);
        vga_draw_rectangle(x + w + 3, y + 4 + i, 1, 6, PT_AMBER);
    }
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
