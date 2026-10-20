#include "u8g2.h"
/* st7302 */
/* st7302 1 */
void u8g2_Setup_st7302_122x250_1(u8g2_t *u8g2, const u8g2_cb_t *rotation, u8x8_msg_cb byte_cb, u8x8_msg_cb gpio_and_delay_cb)
{
  uint8_t tile_buf_height;
  uint8_t *buf;
  u8g2_SetupDisplay(u8g2, u8x8_d_st7302_122x250, u8x8_cad_011, byte_cb, gpio_and_delay_cb);
  buf = u8g2_m_16_32_1(&tile_buf_height);
  u8g2_SetupBuffer(u8g2, buf, tile_buf_height, u8g2_ll_hvline_horizontal_right_lsb, rotation);
}
/* st7302 2 */
void u8g2_Setup_st7302_122x250_2(u8g2_t *u8g2, const u8g2_cb_t *rotation, u8x8_msg_cb byte_cb, u8x8_msg_cb gpio_and_delay_cb)
{
  uint8_t tile_buf_height;
  uint8_t *buf;
  u8g2_SetupDisplay(u8g2, u8x8_d_st7302_122x250, u8x8_cad_011, byte_cb, gpio_and_delay_cb);
  buf = u8g2_m_16_32_2(&tile_buf_height);
  u8g2_SetupBuffer(u8g2, buf, tile_buf_height, u8g2_ll_hvline_horizontal_right_lsb, rotation);
}
/* st7302 f */
void u8g2_Setup_st7302_122x250_f(u8g2_t *u8g2, const u8g2_cb_t *rotation, u8x8_msg_cb byte_cb, u8x8_msg_cb gpio_and_delay_cb)
{
  uint8_t tile_buf_height;
  uint8_t *buf;
  u8g2_SetupDisplay(u8g2, u8x8_d_st7302_122x250, u8x8_cad_011, byte_cb, gpio_and_delay_cb);
  buf = u8g2_m_16_32_f(&tile_buf_height);
  u8g2_SetupBuffer(u8g2, buf, tile_buf_height, u8g2_ll_hvline_horizontal_right_lsb, rotation);
}
/* st7305 */
/* st7305 1 */
void u8g2_Setup_st7305_122x250_1(u8g2_t *u8g2, const u8g2_cb_t *rotation, u8x8_msg_cb byte_cb, u8x8_msg_cb gpio_and_delay_cb)
{
  uint8_t tile_buf_height;
  uint8_t *buf;
  u8g2_SetupDisplay(u8g2, u8x8_d_st7305_122x250, u8x8_cad_011, byte_cb, gpio_and_delay_cb);
  buf = u8g2_m_16_32_1(&tile_buf_height);
  u8g2_SetupBuffer(u8g2, buf, tile_buf_height, u8g2_ll_hvline_horizontal_right_lsb, rotation);
}
/* st7305 2 */
void u8g2_Setup_st7305_122x250_2(u8g2_t *u8g2, const u8g2_cb_t *rotation, u8x8_msg_cb byte_cb, u8x8_msg_cb gpio_and_delay_cb)
{
  uint8_t tile_buf_height;
  uint8_t *buf;
  u8g2_SetupDisplay(u8g2, u8x8_d_st7305_122x250, u8x8_cad_011, byte_cb, gpio_and_delay_cb);
  buf = u8g2_m_16_32_2(&tile_buf_height);
  u8g2_SetupBuffer(u8g2, buf, tile_buf_height, u8g2_ll_hvline_horizontal_right_lsb, rotation);
}
/* st7305 f */
void u8g2_Setup_st7305_122x250_f(u8g2_t *u8g2, const u8g2_cb_t *rotation, u8x8_msg_cb byte_cb, u8x8_msg_cb gpio_and_delay_cb)
{
  uint8_t tile_buf_height;
  uint8_t *buf;
  u8g2_SetupDisplay(u8g2, u8x8_d_st7305_122x250, u8x8_cad_011, byte_cb, gpio_and_delay_cb);
  buf = u8g2_m_16_32_f(&tile_buf_height);
  u8g2_SetupBuffer(u8g2, buf, tile_buf_height, u8g2_ll_hvline_horizontal_right_lsb, rotation);
}
/* st7305 */
/* st7305 1 */
void u8g2_Setup_st7305_200x200_1(u8g2_t *u8g2, const u8g2_cb_t *rotation, u8x8_msg_cb byte_cb, u8x8_msg_cb gpio_and_delay_cb)
{
  uint8_t tile_buf_height;
  uint8_t *buf;
  u8g2_SetupDisplay(u8g2, u8x8_d_st7305_200x200, u8x8_cad_011, byte_cb, gpio_and_delay_cb);
  buf = u8g2_m_26_25_1(&tile_buf_height);
  u8g2_SetupBuffer(u8g2, buf, tile_buf_height, u8g2_ll_hvline_horizontal_right_lsb, rotation);
}
/* st7305 2 */
void u8g2_Setup_st7305_200x200_2(u8g2_t *u8g2, const u8g2_cb_t *rotation, u8x8_msg_cb byte_cb, u8x8_msg_cb gpio_and_delay_cb)
{
  uint8_t tile_buf_height;
  uint8_t *buf;
  u8g2_SetupDisplay(u8g2, u8x8_d_st7305_200x200, u8x8_cad_011, byte_cb, gpio_and_delay_cb);
  buf = u8g2_m_26_25_2(&tile_buf_height);
  u8g2_SetupBuffer(u8g2, buf, tile_buf_height, u8g2_ll_hvline_horizontal_right_lsb, rotation);
}
/* st7305 f */
void u8g2_Setup_st7305_200x200_f(u8g2_t *u8g2, const u8g2_cb_t *rotation, u8x8_msg_cb byte_cb, u8x8_msg_cb gpio_and_delay_cb)
{
  uint8_t tile_buf_height;
  uint8_t *buf;
  u8g2_SetupDisplay(u8g2, u8x8_d_st7305_200x200, u8x8_cad_011, byte_cb, gpio_and_delay_cb);
  buf = u8g2_m_26_25_f(&tile_buf_height);
  u8g2_SetupBuffer(u8g2, buf, tile_buf_height, u8g2_ll_hvline_horizontal_right_lsb, rotation);
}
/* st7305 */
/* st7305 1 */
void u8g2_Setup_st7305_168x384_1(u8g2_t *u8g2, const u8g2_cb_t *rotation, u8x8_msg_cb byte_cb, u8x8_msg_cb gpio_and_delay_cb)
{
  uint8_t tile_buf_height;
  uint8_t *buf;
  u8g2_SetupDisplay(u8g2, u8x8_d_st7305_168x384, u8x8_cad_011, byte_cb, gpio_and_delay_cb);
  buf = u8g2_m_21_48_1(&tile_buf_height);
  u8g2_SetupBuffer(u8g2, buf, tile_buf_height, u8g2_ll_hvline_horizontal_right_lsb, rotation);
}
/* st7305 2 */
void u8g2_Setup_st7305_168x384_2(u8g2_t *u8g2, const u8g2_cb_t *rotation, u8x8_msg_cb byte_cb, u8x8_msg_cb gpio_and_delay_cb)
{
  uint8_t tile_buf_height;
  uint8_t *buf;
  u8g2_SetupDisplay(u8g2, u8x8_d_st7305_168x384, u8x8_cad_011, byte_cb, gpio_and_delay_cb);
  buf = u8g2_m_21_48_2(&tile_buf_height);
  u8g2_SetupBuffer(u8g2, buf, tile_buf_height, u8g2_ll_hvline_horizontal_right_lsb, rotation);
}
/* st7305 f */
void u8g2_Setup_st7305_168x384_f(u8g2_t *u8g2, const u8g2_cb_t *rotation, u8x8_msg_cb byte_cb, u8x8_msg_cb gpio_and_delay_cb)
{
  uint8_t tile_buf_height;
  uint8_t *buf;
  u8g2_SetupDisplay(u8g2, u8x8_d_st7305_168x384, u8x8_cad_011, byte_cb, gpio_and_delay_cb);
  buf = u8g2_m_21_48_f(&tile_buf_height);
  u8g2_SetupBuffer(u8g2, buf, tile_buf_height, u8g2_ll_hvline_horizontal_right_lsb, rotation);
}
/* st7305 yuying 200x200 f*/
void u8g2_Setup_st7305_yuying_200x200_f(u8g2_t *u8g2, const u8g2_cb_t *rotation, u8x8_msg_cb byte_cb, u8x8_msg_cb gpio_and_delay_cb)
{
  uint8_t tile_buf_height;
  uint8_t *buf;
  u8g2_SetupDisplay(u8g2, u8x8_d_st7305_200x200_yuying, u8x8_cad_011, byte_cb, gpio_and_delay_cb);
  buf = u8g2_m_25_25_f(&tile_buf_height);
  u8g2_SetupBuffer(u8g2, buf, tile_buf_height, u8g2_ll_hvline_vertical_top_lsb, rotation);
}
/* st7305 yuying 168x384 f*/
//void u8g2_Setup_st7305_yuying_168x384_f(u8g2_t *u8g2, const u8g2_cb_t *rotation, u8x8_msg_cb byte_cb, u8x8_msg_cb gpio_and_delay_cb)
//{
//  uint8_t tile_buf_height;
//  uint8_t *buf;
//  u8g2_SetupDisplay(u8g2, u8x8_d_st7305_168x384_yuying, u8x8_cad_011, byte_cb, gpio_and_delay_cb);
//  buf = u8g2_m_21_48_f(&tile_buf_height);
//  u8g2_SetupBuffer(u8g2, buf, tile_buf_height, u8g2_ll_hvline_vertical_top_lsb, rotation);
//}
void u8g2_Setup_st7305_yuying_168x384_f(u8g2_t *u8g2, const u8g2_cb_t *rotation, u8x8_msg_cb byte_cb, u8x8_msg_cb gpio_and_delay_cb)
{
  uint8_t tile_buf_height;
  uint8_t *buf;
  u8g2_SetupDisplay(u8g2, u8x8_d_st7305_168x384_yuying, u8x8_cad_011, byte_cb, gpio_and_delay_cb);
  buf = u8g2_m_21_48_f(&tile_buf_height);
  u8g2_SetupBuffer(u8g2, buf, tile_buf_height, u8g2_ll_hvline_vertical_top_lsb, rotation);
}
/* st7306 yuying 300x400 f*/
void u8g2_Setup_st7306_300x400_f(u8g2_t *u8g2, const u8g2_cb_t *rotation,
                                 u8x8_msg_cb byte_cb, u8x8_msg_cb gpio_and_delay_cb)
{
    uint8_t tile_buf_height;
    uint8_t *buf;

    // 使用新的显示驱动 u8x8_d_st7306_300x400（需自行实现）
    u8g2_SetupDisplay(u8g2, u8x8_d_st7306_300x400, u8x8_cad_011, byte_cb, gpio_and_delay_cb);

    // 分配全屏缓冲区
    buf = u8g2_m_38_50_f(&tile_buf_height);
    u8g2_SetupBuffer(u8g2, buf, tile_buf_height, u8g2_ll_hvline_vertical_top_lsb, rotation);
}
