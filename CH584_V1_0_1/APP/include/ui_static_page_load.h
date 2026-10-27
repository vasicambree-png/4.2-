#ifndef UI_STATIC_PAGE_LOAD_H
#define UI_STATIC_PAGE_LOAD_H

#if UI_STATIC_PAGES
#include "ui_static_page_assets.h"

/* Packets: low 7 bits + 1 bytes; high bit repeats the following byte.
 * Each independently compressed chunk restores exactly five tile rows.
 * Decoding writes directly into u8g2's existing 1520-byte buffer. */
static uint8_t ui_static_page_load(uint8_t key)
{
    uint16_t chunk;
    const uint8_t *src, *end;
    uint8_t *dst = u8g2.tile_buf_ptr;
    uint16_t remaining = 1520u;
    if (u8g2_GetU8x8(&u8g2)->display_info->tile_width != 38u ||
        key >= UI_BG_COUNT || u8g2.tile_curr_row >= 50u ||
        u8g2.tile_curr_row % 5u != 0u || u8g2.tile_buf_height != 5u) return 0u;
    chunk = ui_bg_page_chunks[key][u8g2.tile_curr_row / 5u];
    src = ui_bg_data + ui_bg_offsets[chunk];
    end = ui_bg_data + ui_bg_offsets[chunk + 1u];
    while (remaining != 0u)
    {
        uint8_t packet;
        uint16_t count;
        if (src == end) return 0u;
        packet = *src++;
        count = (uint16_t)((packet & 127u) + 1u);
        if (count > remaining) return 0u;
        if (packet & 128u) {
            if (src == end) return 0u;
            memset(dst, *src++, count);
        }
        else {
            if ((uint32_t)(end - src) < count) return 0u;
            memcpy(dst, src, count); src += count;
        }
        dst += count;
        remaining = (uint16_t)(remaining - count);
    }
    return (uint8_t)(src == end);
}
#endif

#endif
