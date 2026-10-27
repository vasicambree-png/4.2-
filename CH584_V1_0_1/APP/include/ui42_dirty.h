#ifndef UI42_DIRTY_H
#define UI42_DIRTY_H

#include "u8g2.h"

/* ---------------------------------------------------------------------------
 * 4.2" 屏（ST7306 600 source x 400 gate）脏行局部刷新
 * ---------------------------------------------------------------------------
 * 整帧缓冲 = 75 tile 列 x 50 tile 行 x 8 = 30000 B。改前每次 UI_Control() 都调
 * u8g2_SendBuffer()，把 30000 B 全部推上 SPI（阻塞），一帧 ≈100 ms 量级。
 *
 * 本模块把"整屏发"换成"只发变化的 tile 行"：
 *   1) 驱动的 DRAW_TILE 早就支持单行：x_pos==0 && cnt==75 && y_pos<50 就放行
 *      （u8g2\u8x8_d_st7305.c:1355-1360）；
 *   2) u8g2_send_buffer() 本来就是"buffer 第 i 行 -> display 第 i 行"
 *      （u8g2\u8g2_buffer.c:79-89，tile_curr_row 恒 0），
 *      旋转（U8G2_R3 / ui42_physical_cb）不改变这个行索引映射；
 *      ⇒ 单行发送与整屏发送在该行上**逐位相同**，不会错行；
 *   3) 逐行算 CRC16 与上次发出去的值比较，只发变的行。
 *
 * 成本：50 行 x 600 B = 30000 B 的表查（≈2 ms @78MHz），不上总线；
 *       一屏里只有几个数字变化时，实际只发 3~15 行（600~3000 B）。
 *
 * 自愈：CRC16 理论上可能撞；因此每 UI42_DIRTY_FULL_EVERY 次刷新强制整屏一次，
 *       并且 u8g2Init() 之后由调用方 ui42_invalidate_full() 作废缓存。
 * ------------------------------------------------------------------------- */

#define UI42_DIRTY_ROWS        50u  /* 400 gate / 8 = 50 tile 行 */
#define UI42_DIRTY_FULL_EVERY  16u  /* 每 16 次刷新强制整屏一次（哈希自愈） */

static uint16_t s_ui42_row_crc[UI42_DIRTY_ROWS];
static uint8_t  s_ui42_all_dirty = 1u;
static uint8_t  s_ui42_since_full;

/* 诊断计数（只写不读也行，供串口日志用） */
static uint16_t s_ui42_rows_last;
static uint32_t s_ui42_flush_cnt;

/* CRC16-CCITT（半字节查表，32 B 常量） */
static uint16_t ui42_crc16(const uint8_t *p, uint16_t n)
{
    static const uint16_t tab[16] =
    {
        0x0000u, 0x1021u, 0x2042u, 0x3063u, 0x4084u, 0x50A5u, 0x60C6u, 0x70E7u,
        0x8108u, 0x9129u, 0xA14Au, 0xB16Bu, 0xC18Cu, 0xD1ADu, 0xE1CEu, 0xF1EFu
    };
    uint16_t crc = 0xFFFFu;

    while (n-- > 0u)
    {
        uint8_t b = *p++;

        crc = (uint16_t)((uint16_t)(crc << 4) ^ tab[((crc >> 12) ^ (uint16_t)(b >> 4)) & 0x0Fu]);
        crc = (uint16_t)((uint16_t)(crc << 4) ^ tab[((crc >> 12) ^ (uint16_t)(b & 0x0Fu)) & 0x0Fu]);
    }
    return crc;
}

/* 作废缓存：下一次 flush 一定整屏发一遍 */
static void ui42_invalidate_full(void)
{
    s_ui42_all_dirty = 1u;
}

/* 把 buffer 里"变了的行"送屏；返回本次真正发出去的行数。
   几何全部取自 display_info（不硬编码），并夹到数组上限；
   若这屏的行数超过 UI42_DIRTY_ROWS，则退回整屏 u8g2_SendBuffer()。 */
static uint16_t ui42_flush_dirty(u8g2_t *u8g2)
{
    u8x8_t  *u8x8   = u8g2_GetU8x8(u8g2);
    uint8_t  cols   = u8x8->display_info->tile_width;
    uint8_t  rows   = u8x8->display_info->tile_height;
    uint16_t stride = (uint16_t)((uint16_t)cols * 8u);
    uint8_t *buf    = u8g2_GetBufferPtr(u8g2);
    uint8_t  full   = s_ui42_all_dirty;
    uint16_t sent   = 0u;
    uint8_t  row;

    if ((buf == NULL) || (cols == 0u) || (rows == 0u) || (rows > UI42_DIRTY_ROWS))
    {
        u8g2_SendBuffer(u8g2);          /* 未知几何：退回整屏，绝不猜行号 */
        return rows;
    }

    if (full == 0u)
    {
        s_ui42_since_full++;
        if (s_ui42_since_full >= UI42_DIRTY_FULL_EVERY)
        {
            full = 1u;                  /* 周期性整屏：给 CRC 撞车兜底 */
        }
    }

    for (row = 0u; row < rows; row++)
    {
        uint8_t       *p = buf + (uint16_t)((uint16_t)row * stride);
        uint16_t       h = ui42_crc16(p, stride);

        if ((full != 0u) || (h != s_ui42_row_crc[row]))
        {
            s_ui42_row_crc[row] = h;
            /* 与 u8g2_send_tile_row() 完全同一条调用（u8g2_buffer.c:51-64）：
               单行、整行宽，驱动侧只认这个形状。 */
            (void)u8x8_DrawTile(u8x8, 0u, row, cols, p);
            sent++;
        }
    }

    if (full != 0u)
    {
        s_ui42_all_dirty = 0u;
        s_ui42_since_full = 0u;
    }
    s_ui42_rows_last = sent;
    s_ui42_flush_cnt++;

    return sent;
}

#endif /* UI42_DIRTY_H */
