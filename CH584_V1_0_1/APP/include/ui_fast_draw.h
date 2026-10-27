#ifndef UI_FAST_DRAW_H
#define UI_FAST_DRAW_H

#include "u8g2.h"

#ifdef __HIGH_CODE
#define UI_FAST_CODE __HIGH_CODE
#else
#define UI_FAST_CODE
#endif

/* Only accelerate full vertical-LSB R3 buffers and the explicit 4.2-inch
 * physical callback. Other callbacks, partial buffers and clipping keep u8g2. */
static inline uint8_t ui_fast_y_scale(const u8g2_t *s)
{
#ifdef UI_PHYSICAL_42_H
    if (s->cb == &ui42_physical_cb) return 2u;
#endif
    return 1u;
}

static inline uint8_t ui_fast_can_draw(const u8g2_t *s, u8g2_uint_t x,
    u8g2_uint_t y, u8g2_uint_t w, u8g2_uint_t h)
{
#ifdef UI_FAST_DRAW_DISABLE
    return 0u;
#else
    uint8_t scale = 1u;
    if (s->cb != U8G2_R3)
    {
#ifdef UI_PHYSICAL_42_H
        if (s->cb != &ui42_physical_cb || s->width != 400u ||
            s->height != 300u || s->pixel_buf_width != 600u ||
            s->pixel_buf_height != 400u) return 0u;
        scale = 2u;
#else
        return 0u;
#endif
    }
    if (!w || !h || !s->tile_buf_ptr || s->draw_color > 2u ||
        s->ll_hvline != u8g2_ll_hvline_vertical_top_lsb ||
        !s->width || !s->height || s->width != s->pixel_buf_height ||
        (uint32_t)s->height * scale != s->pixel_buf_width ||
        (uint32_t)s->tile_buf_height * 8u != s->pixel_buf_height ||
        /* The reference low-level renderer uses a 16-bit buffer offset. */
        (uint32_t)s->pixel_buf_width * s->tile_buf_height > 65535u ||
        s->tile_curr_row || s->pixel_curr_row || s->buf_y0 ||
        s->buf_y1 != s->pixel_buf_height)
        return 0u;
#ifdef U8G2_WITH_CLIP_WINDOW_SUPPORT
    if (!s->is_page_clip_window_intersection) return 0u;
#endif
    return x >= s->user_x0 && y >= s->user_y0 &&
           (uint32_t)x + w <= s->width && (uint32_t)y + h <= s->height &&
           (uint32_t)x + w <= s->user_x1 && (uint32_t)y + h <= s->user_y1;
#endif
}

static inline uint8_t ui_fast_reverse8(uint8_t v)
{
    v = (uint8_t)(((v & 0x55u) << 1) | ((v >> 1) & 0x55u));
    v = (uint8_t)(((v & 0x33u) << 2) | ((v >> 2) & 0x33u));
    return (uint8_t)((v << 4) | (v >> 4));
}

static inline void ui_fast_bitmap_byte(uint8_t *dst, uint8_t bits,
    uint8_t mask, uint8_t color, uint8_t transparent)
{
    if (transparent)
    {
        if (color == 0u) *dst &= (uint8_t)~bits;
        else if (color == 1u) *dst |= bits;
        else *dst ^= bits;
    }
    else
    {
        uint8_t value;
        if (color == 0u) value = (uint8_t)(mask & (uint8_t)~bits);
        else if (color == 1u) value = bits;
        else value = (uint8_t)((uint8_t)~*dst & bits);
        *dst = (uint8_t)((*dst & (uint8_t)~mask) | value);
    }
}

UI_FAST_CODE
static void ui_fast_DrawXBMP(u8g2_t *s, u8g2_uint_t x, u8g2_uint_t y,
    u8g2_uint_t w, u8g2_uint_t h, const uint8_t *bitmap)
{
    uint16_t row, col, stride;
    uint8_t scale;
    if (!ui_fast_can_draw(s, x, y, w, h))
    {
        u8g2_DrawXBMP(s, x, y, w, h, bitmap);
        return;
    }
    scale = ui_fast_y_scale(s);
    stride = (uint16_t)((w + 7u) >> 3);
    for (row = 0u; row < h; row++)
    {
        for (col = 0u; col < w; col += 8u)
        {
            uint8_t n = (uint8_t)((w - col < 8u) ? w - col : 8u);
            uint16_t low = (uint16_t)(s->width - x - col - n);
            uint8_t shift = (uint8_t)(low & 7u);
            uint16_t mask = (uint16_t)(((1u << n) - 1u) << shift);
            uint16_t bits = (uint16_t)((uint16_t)(ui_fast_reverse8(
                u8x8_pgm_read(bitmap + row * stride + (col >> 3))) >> (8u - n)) << shift);
            uint8_t *dst = s->tile_buf_ptr +
                (uint32_t)(low >> 3) * s->pixel_buf_width + (uint32_t)(y + row) * scale;
            uint8_t copy;
            for (copy = 0u; copy < scale; copy++)
            {
                ui_fast_bitmap_byte(dst + copy, (uint8_t)bits, (uint8_t)mask,
                                    s->draw_color, s->bitmap_transparency);
                if (mask >> 8)
                    ui_fast_bitmap_byte(dst + copy + s->pixel_buf_width,
                        (uint8_t)(bits >> 8), (uint8_t)(mask >> 8),
                        s->draw_color, s->bitmap_transparency);
            }
        }
    }
}

UI_FAST_CODE
static void ui_fast_DrawBox(u8g2_t *s, u8g2_uint_t x, u8g2_uint_t y,
    u8g2_uint_t w, u8g2_uint_t h)
{
    uint16_t low, high, page, col;
    uint8_t scale;
    if (!ui_fast_can_draw(s, x, y, w, h))
    {
        u8g2_DrawBox(s, x, y, w, h);
        return;
    }
    scale = ui_fast_y_scale(s);
    low = (uint16_t)(s->width - x - w);
    high = (uint16_t)(s->width - x);
    for (page = (uint16_t)(low >> 3); page <= (uint16_t)((high - 1u) >> 3); page++)
    {
        uint16_t base = (uint16_t)(page << 3);
        uint8_t first = (uint8_t)((low > base) ? low - base : 0u);
        uint8_t last = (uint8_t)((high < base + 8u) ? high - base : 8u);
        uint8_t mask = (uint8_t)(((1u << last) - 1u) & ~((1u << first) - 1u));
        uint8_t *dst = s->tile_buf_ptr +
            (uint32_t)page * s->pixel_buf_width + (uint32_t)y * scale;
        for (col = 0u; col < (uint32_t)h * scale; col++)
        {
            if (s->draw_color == 0u) dst[col] &= (uint8_t)~mask;
            else if (s->draw_color == 1u) dst[col] |= mask;
            else dst[col] ^= mask;
        }
    }
}

#undef UI_FAST_CODE
#endif
