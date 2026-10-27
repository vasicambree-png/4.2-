#ifndef UI_HOME_PHYSICAL_H
#define UI_HOME_PHYSICAL_H

/* Keep every homepage element inside the panel's fixed black edge. These
 * helpers map inclusive pixel positions, preserving thin strokes and borders. */
#ifndef UI_HOME_INSET
#define UI_HOME_INSET 8u
#endif
#define UI_HOME_INNER_W (400u - 2u * UI_HOME_INSET)
#define UI_HOME_INNER_H (300u - 2u * UI_HOME_INSET)
#define UI_HOME_METADATA_BASELINE 286u
#define UI_HOME_ROW_BASELINE 80u
#define UI_HOME_ROW_PITCH 20u
#define UI_HOME_COLUMN_OFFSET 185u
#include "ui_home_frame_asset.h"

static uint16_t ui_home_panel_x(uint16_t x)
{
    return (uint16_t)(UI_HOME_INSET + ((uint32_t)x * (UI_HOME_INNER_W - 1u) + 199u) / 399u);
}

static uint16_t ui_home_panel_y(uint16_t y)
{
    return (uint16_t)(UI_HOME_INSET + ((uint32_t)y * (UI_HOME_INNER_H - 1u) + 149u) / 299u);
}

/* Homepage only: one 400x300 physical pixel occupies two vertical source
 * coordinates in this module's 400x600 driver canvas. Keep font proportions
 * and page-buffer clipping; do not change the menus' legacy projection. */
static void ui_home_dimension(u8g2_t *s)
{
    u8g2_cb_r3.update_dimension(s);
    s->height = SCREEN_PHYSICAL_HEIGHT;
}

static void ui_home_window(u8g2_t *s)
{
    uint16_t left = (uint16_t)(400u - s->buf_y1);
    uint16_t right = (uint16_t)(400u - s->buf_y0);
    u8g2_cb_r3.update_page_win(s);
    /* Include source pixels whose scaled ink lands on this physical page. */
    s->user_x0 = left <= UI_HOME_INSET ? 0u :
        (uint16_t)((uint32_t)(left - UI_HOME_INSET) * 399u / (UI_HOME_INNER_W - 1u));
    if (s->user_x0) --s->user_x0;
    s->user_x1 = right <= UI_HOME_INSET ? 0u :
        right >= 400u - UI_HOME_INSET ? 400u :
        (uint16_t)(((uint32_t)(right - UI_HOME_INSET) * 399u + UI_HOME_INNER_W - 2u) /
                   (UI_HOME_INNER_W - 1u) + 2u);
    if (s->user_x1 > 400u) s->user_x1 = 400u;
#ifdef U8G2_WITH_CLIP_WINDOW_SUPPORT
    s->is_page_clip_window_intersection = u8g2_IsIntersection(
        s, s->clip_x0, s->clip_y0, s->clip_x1, s->clip_y1);
    if (s->user_x0 < s->clip_x0) s->user_x0 = s->clip_x0;
    if (s->user_x1 > s->clip_x1) s->user_x1 = s->clip_x1;
#endif
}

static void ui_home_line(u8g2_t *s, u8g2_uint_t x, u8g2_uint_t y,
                         u8g2_uint_t length, uint8_t direction)
{
    uint16_t x0, x1, y0, y1;
    uint16_t left = (uint16_t)(400u - s->buf_y1);
    uint16_t right = (uint16_t)(400u - s->buf_y0);
    if (!length) return;
    x0 = ui_home_panel_x(x);
    x1 = (uint16_t)(ui_home_panel_x((uint16_t)(x + (direction == 0u ? length - 1u : 0u))) + 1u);
    y0 = ui_home_panel_y(y);
    y1 = (uint16_t)(ui_home_panel_y((uint16_t)(y + (direction == 1u ? length - 1u : 0u))) + 1u);
    if (x0 < left) x0 = left;
    if (x1 > right) x1 = right;
    if (x0 >= x1) return;
    if (direction == 0u) {
        u8g2_cb_r3.draw_l90(s, x0, (uint16_t)(2u * y0), (uint16_t)(x1 - x0), 0u);
        u8g2_cb_r3.draw_l90(s, x0, (uint16_t)(2u * y0 + 1u), (uint16_t)(x1 - x0), 0u);
    } else {
        u8g2_cb_r3.draw_l90(s, x0, (uint16_t)(2u * y0),
                          (uint16_t)(2u * (y1 - y0)), 1u);
    }
}

static const u8g2_cb_t ui_home_physical_cb = {
    ui_home_dimension, ui_home_window, ui_home_line
};

static void ui_home_right(uint16_t right, uint16_t baseline,
                           const char *text, uint8_t size)
{
    uint16_t width = ui_text_width(text, size);
    ui_text_draw(width < right ? (uint16_t)(right - width) : 0u,
                 baseline, text, size);
}

static void ui_home_metadata(const data_main *m)
{
    char fields[3][32];
    char destination[8];
    uint16_t width, left;
    uint8_t i, size = 14u;
    if (m->send_host_num == 122u) snprintf(destination, sizeof(destination), "分站");
    else if (m->send_host_num == 121u) snprintf(destination, sizeof(destination), "无");
    else if (m->send_host_num == 0u) snprintf(destination, sizeof(destination), "中继");
    else snprintf(destination, sizeof(destination), "%u", (unsigned)m->send_host_num);
    snprintf(fields[0], sizeof(fields[0]), "本机号:%u-->%s", (unsigned)m->host_num, destination);
    snprintf(fields[1], sizeof(fields[1]), "状态:%s", m->state ? "开机" : "关机");
    snprintf(fields[2], sizeof(fields[2]), "分站号:%u", (unsigned)m->sub_num);
    /* Three equal 128-pixel cells; the middle cell stays centered even when
     * either address changes digit count. Use one size for the entire row. */
    for (i = 0u; i < 3u; ++i)
        if (ui_text_width(fields[i], size) > 120u) size = 11u;
    for (i = 0u; i < 3u; ++i) {
        width = ui_text_width(fields[i], size);
        left = (uint16_t)(8u + 128u * i);
        ui_text_draw((uint16_t)(left + (128u - width) / 2u), UI_HOME_METADATA_BASELINE, fields[i], size);
    }
}

/* The supplied artwork is converted offline to native physical pixels. Drawing
 * it through the inset scaler again would merge the thin white corner inlays. */
static void ui_home_reference_frame(void)
{
    u8g2_SetDisplayRotation(&u8g2, &ui42_physical_cb);
    u8g2_DrawXBMP(&u8g2, UI_HOME_INSET, UI_HOME_INSET,
                  UI_HOME_FRAME_WIDTH, UI_HOME_FRAME_HEIGHT, ui_home_frame_bits);
    u8g2_SetDisplayRotation(&u8g2, &ui_home_physical_cb);
}

static void ui_u0_home(void)
{
    u8g2_t saved = u8g2;
    const data_main *m = &Data_list1.UI_main;
    char text[48], label[8], value[24];
    uint8_t ch, row, size, level = ui_u0_signal_level(m->Lora_rssi);
    uint16_t dot;

    u8g2.cb = &ui_home_physical_cb;
    u8g2.cb->update_dimension(&u8g2);
    u8g2.cb->update_page_win(&u8g2);
    u8g2_SetDrawColor(&u8g2, 1u);
    u8g2_SetBitmapMode(&u8g2, 1u);
    ui_home_reference_frame();
    u8g2_DrawXBMP(&u8g2, 14u, 13u, UI_HOME_FRAME_BRAND_W,
                  UI_HOME_FRAME_BRAND_H, ui_home_frame_brand);
    u8g2_DrawXBMP(&u8g2, 110u, 10u, UI_HOME_FRAME_TITLE_W,
                  UI_HOME_FRAME_TITLE_H, ui_home_frame_title);
    snprintf(text, sizeof(text), "%u.%02uV", (unsigned)(m->vbat / 1000u),
             (unsigned)((m->vbat % 1000u) / 10u));
    /* Native pixels preserve the radio arcs and the slightly larger digits.
     * Reserve a gap after the icon; long readings keep the existing fallback. */
    u8g2_SetDisplayRotation(&u8g2, &ui42_physical_cb);
    size = ui_text_width(text, 16u) <= 44u ? 16u :
           ui_text_width(text, 14u) <= 44u ? 14u : 11u;
    ui_home_right(379u, size == 16u ? 46u : size == 14u ? 45u : 43u, text, size);
    u8g2_DrawXBMP(&u8g2, 307u, 27u, UI_HOME_RADIO_WIFI_W,
                  UI_HOME_RADIO_WIFI_H, ui_home_radio_wifi[level]);
    u8g2_SetDisplayRotation(&u8g2, &ui_home_physical_cb);

    /* Keep dotted rules inside each separate pane. */
    for (row = 0u; row < 9u; ++row)
        for (dot = 26u; dot < 190u; dot = (uint16_t)(dot + 4u)) {
            uint16_t y = (uint16_t)(UI_HOME_ROW_BASELINE + 3u + UI_HOME_ROW_PITCH * row);
            u8g2_DrawBox(&u8g2, dot, y, 2u, 1u);
            u8g2_DrawBox(&u8g2, (uint16_t)(dot + UI_HOME_COLUMN_OFFSET), y, 2u, 1u);
        }
    /* Keep channel glyphs at their native pixel size. Scaling the bitmap with
     * the inset frame merges adjacent raster rows and closes fine strokes.
     * Map only the layout anchors; ui42 preserves every row of each glyph. */
    u8g2_SetDisplayRotation(&u8g2, &ui42_physical_cb);
    for (ch = 0u; ch < MAX_CH_NUM; ++ch) {
        uint16_t x = ch < 10u ? 0u : UI_HOME_COLUMN_OFFSET;
        uint16_t y = ui_home_panel_y((uint16_t)(UI_HOME_ROW_BASELINE + UI_HOME_ROW_PITCH * (ch % 10u)));
        uint16_t raw = m->data[ch];
        Sensor_Tpye type = ui_home_channel_type(ch);
        uint8_t known = (uint8_t)(type > TPYE_NONE && type < TPYE_END);
        uint16_t left = ui_home_panel_x((uint16_t)(x + 51u));
        uint16_t right = ui_home_panel_x((uint16_t)(x + 153u));
        snprintf(label, sizeof(label), "%u", (unsigned)(ch + 1u));
        ui_text_draw((uint16_t)(ui_home_panel_x((uint16_t)(x + 48u)) -
                     ui_text_width(label, 16u)), y, label, 16u);
        if (known) {
            ui_text_draw(left, y, CH_TYPE_NAME[type], 16u);
            left = (uint16_t)(left + ui_text_width(CH_TYPE_NAME[type], 16u) + 7u);
        }
        if (raw == UI_U0_NO_DATA) snprintf(value, sizeof(value), "--");
        else ch_disp_format(value, sizeof(value), type, raw);
        size = ui_text_width(value, 18u) <= right - left ? 18u :
               ui_text_width(value, 16u) <= right - left ? 16u : 14u;
        ui_home_right(right, y, value, size);
        if (known && raw != UI_U0_NO_DATA && CH_TYPE_UNIT[type][0])
            ui_text_draw(ui_home_panel_x((uint16_t)(x + 158u)), y, CH_TYPE_UNIT[type], 16u);
    }
    u8g2_SetDisplayRotation(&u8g2, &ui_home_physical_cb);
    ui_home_metadata(m);
    /* Restore the caller's legacy clipping, dimensions and drawing state. */
    u8g2 = saved;
}

#endif
