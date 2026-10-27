#ifndef UI_LEGACY_42_PROJECTION_H
#define UI_LEGACY_42_PROJECTION_H

/* Reuse the 384x168 UI, enlarging layout space while keeping glyph/icon shape. */
static uint16_t ui42_edge_x(uint16_t x)
{ return (uint16_t)(((uint32_t)x * 400u + 383u) / 384u); }
static uint16_t ui42_edge_y(uint16_t y)
{ return (uint16_t)(((uint32_t)y * SCREEN_HEIGHT + 167u) / 168u); }

static void ui42_legacy_dimension(u8g2_t *s)
{
    u8g2_cb_r3.update_dimension(s);
    s->width = 384u;
    s->height = 168u;
}

static void ui42_legacy_window(u8g2_t *s)
{
    uint16_t left = (uint16_t)(400u - s->buf_y1);
    uint16_t right = (uint16_t)(400u - s->buf_y0);
    /* Include source pixels straddling a physical page boundary. */
    s->user_x0 = (uint16_t)((uint32_t)left * 384u / 400u);
    s->user_x1 = (uint16_t)(((uint32_t)right * 384u + 399u) / 400u);
    s->user_y0 = 0u;
    s->user_y1 = 168u;
#ifdef U8G2_WITH_CLIP_WINDOW_SUPPORT
    s->is_page_clip_window_intersection = u8g2_IsIntersection(
        s, s->clip_x0, s->clip_y0, s->clip_x1, s->clip_y1);
    if (s->user_x0 < s->clip_x0) s->user_x0 = s->clip_x0;
    if (s->user_x1 > s->clip_x1) s->user_x1 = s->clip_x1;
    if (s->user_y0 < s->clip_y0) s->user_y0 = s->clip_y0;
    if (s->user_y1 > s->clip_y1) s->user_y1 = s->clip_y1;
#endif
}

static void ui42_legacy_line(u8g2_t *s, u8g2_uint_t x, u8g2_uint_t y,
                              u8g2_uint_t len, uint8_t dir)
{
    uint16_t x0 = ui42_edge_x(x), y0 = ui42_edge_y(y);
    uint16_t x1 = ui42_edge_x((uint16_t)(x + (dir == 0u ? len : 1u)));
    uint16_t y1 = ui42_edge_y((uint16_t)(y + (dir == 1u ? len : 1u)));
    uint16_t left = (uint16_t)(400u - s->buf_y1);
    uint16_t right = (uint16_t)(400u - s->buf_y0);
    if (x0 < left) x0 = left;
    if (x1 > right) x1 = right;
    if (y1 > SCREEN_HEIGHT) y1 = SCREEN_HEIGHT;
    if (x0 >= x1 || y0 >= y1) return;
    s->width = 400u;
    s->height = SCREEN_HEIGHT;
    while (y0 < y1)
        u8g2_cb_r3.draw_l90(s, x0, y0++, (uint16_t)(x1 - x0), 0u);
    s->width = 384u;
    s->height = 168u;
}

static const u8g2_cb_t ui42_legacy_cb = {
    ui42_legacy_dimension, ui42_legacy_window, ui42_legacy_line
};

static void ui42_legacy_bitmap(u8g2_t *s, u8g2_uint_t x, u8g2_uint_t y,
                                u8g2_uint_t w, u8g2_uint_t h, const uint8_t *bits)
{
    if (s->cb == &ui42_legacy_cb && bits != ui_menu_return_page)
    {
        /* Native glyphs/art stay proportional, centered in their enlarged slot.
         * A local context shares the physical buffer, without changing the
         * logical clipping window used by the surrounding legacy drawing. */
        u8g2_t native = *s;
        uint16_t x0 = ui42_edge_x(x), y0 = ui42_edge_y(y);
        uint16_t slot_w = (uint16_t)(ui42_edge_x((uint16_t)(x + w)) - x0);
        uint16_t slot_h = (uint16_t)(ui42_edge_y((uint16_t)(y + h)) - y0);
        native.cb = U8G2_R3;
        native.cb->update_dimension(&native);
        native.cb->update_page_win(&native);
        u8g2_DrawXBMP(&native, (uint16_t)(x0 + (slot_w - w) / 2u),
                     (uint16_t)(y0 + (slot_h - h) / 2u), w, h, bits);
    }
    else
        /* The return-page image is a full-pane background, not a glyph/icon. */
        ui_fast_DrawXBMP(s, x, y, w, h, bits);
}

/* Scoped to this translation unit after the original fast renderer definition. */
#define ui_fast_DrawXBMP ui42_legacy_bitmap

#endif
