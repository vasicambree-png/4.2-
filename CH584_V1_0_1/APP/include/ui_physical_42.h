#ifndef UI_PHYSICAL_42_H
#define UI_PHYSICAL_42_H

/* 4.2-inch ST7306 physical canvas (400x300).
 * Maps every vertical coordinate to two scan lines on the 400x600 controller,
 * providing a 1:1 square-pixel aspect ratio for menus and subpages. */

static void ui42_physical_dimension(u8g2_t *s)
{
    u8g2_cb_r3.update_dimension(s);
    s->height = SCREEN_PHYSICAL_HEIGHT;
}

static void ui42_physical_window(u8g2_t *s)
{
    u8g2_cb_r3.update_page_win(s);
}

static void ui42_physical_line(u8g2_t *s, u8g2_uint_t x, u8g2_uint_t y,
                              u8g2_uint_t length, uint8_t direction)
{
    if (direction == 0u) {
        u8g2_cb_r3.draw_l90(s, x, (uint16_t)(2u * y), length, 0u);
        u8g2_cb_r3.draw_l90(s, x, (uint16_t)(2u * y + 1u), length, 0u);
    } else {
        u8g2_cb_r3.draw_l90(s, x, (uint16_t)(2u * y),
                          (uint16_t)(2u * length), 1u);
    }
}

static const u8g2_cb_t ui42_physical_cb = {
    ui42_physical_dimension, ui42_physical_window, ui42_physical_line
};

#endif /* UI_PHYSICAL_42_H */
