/* u8g2_d_memory.c */
/* generated code, codebuild, u8g2 project */

#include "u8g2.h"


uint8_t *u8g2_m_16_32_1(uint8_t *page_cnt)
{
  #ifdef U8G2_USE_DYNAMIC_ALLOC
  *page_cnt = 1;
  return 0;
  #else
  static uint8_t buf[128];
  *page_cnt = 1;
  return buf;
  #endif
}
uint8_t *u8g2_m_16_32_2(uint8_t *page_cnt)
{
  #ifdef U8G2_USE_DYNAMIC_ALLOC
  *page_cnt = 2;
  return 0;
  #else
  static uint8_t buf[256];
  *page_cnt = 2;
  return buf;
  #endif
}
uint8_t *u8g2_m_16_32_f(uint8_t *page_cnt)
{
  #ifdef U8G2_USE_DYNAMIC_ALLOC
  *page_cnt = 32;
  return 0;
  #else
  static uint8_t buf[4096];
  *page_cnt = 32;
  return buf;
  #endif
}
uint8_t *u8g2_m_26_25_1(uint8_t *page_cnt)
{
  #ifdef U8G2_USE_DYNAMIC_ALLOC
  *page_cnt = 1;
  return 0;
  #else
  static uint8_t buf[208];
  *page_cnt = 1;
  return buf;
  #endif
}
uint8_t *u8g2_m_26_25_2(uint8_t *page_cnt)
{
  #ifdef U8G2_USE_DYNAMIC_ALLOC
  *page_cnt = 2;
  return 0;
  #else
  static uint8_t buf[416];
  *page_cnt = 2;
  return buf;
  #endif
}
uint8_t *u8g2_m_26_25_f(uint8_t *page_cnt)
{
  #ifdef U8G2_USE_DYNAMIC_ALLOC
  *page_cnt = 25;
  return 0;
  #else
  static uint8_t buf[5200];
  *page_cnt = 25;
  return buf;
  #endif
}
uint8_t *u8g2_m_21_48_1(uint8_t *page_cnt)
{
  #ifdef U8G2_USE_DYNAMIC_ALLOC
  *page_cnt = 1;
  return 0;
  #else
  static uint8_t buf[168];
  *page_cnt = 1;
  return buf;
  #endif
}
uint8_t *u8g2_m_21_48_2(uint8_t *page_cnt)
{
  #ifdef U8G2_USE_DYNAMIC_ALLOC
  *page_cnt = 2;
  return 0;
  #else
  static uint8_t buf[336];
  *page_cnt = 2;
  return buf;
  #endif
}
uint8_t *u8g2_m_21_48_f(uint8_t *page_cnt)
{
  #ifdef U8G2_USE_DYNAMIC_ALLOC
  *page_cnt = 48;
  return 0;
  #else
  static uint8_t buf[8064];
  *page_cnt = 48;
  return buf;
  #endif
}
/* Supplied 4.2-inch module driver: 75 x 50 x 8 = 30000 bytes.
 * Keep the historical helper symbol for source/link compatibility. */
uint8_t *u8g2_m_38_50_f(uint8_t *page_cnt)
{
#ifdef U8G2_USE_DYNAMIC_ALLOC
    *page_cnt = 50;
    return 0;   // 动态分配由调用者处理
#else
    static uint8_t buf[75u * 50u * 8u];   /* 30000 bytes */
    *page_cnt = 50;
    return buf;
#endif
}

/* Five tile rows of the supplied 600 x 400 driver framebuffer.
 * U8g2 FirstPage/NextPage redraws each 3000-byte page. */
uint8_t *u8g2_m_75_50_p5(uint8_t *page_cnt)
{
#ifdef U8G2_USE_DYNAMIC_ALLOC
    *page_cnt = 5u;
    return 0;
#else
    static uint8_t buf[75u * 5u * 8u];   /* 3000 bytes */
    *page_cnt = 5u;
    return buf;
#endif
}
