/********************************** (C) COPYRIGHT *******************************
 * File Name          : app_drv_fifo.c
 * Author             : WCH
 * Version            : V1.1
 * Date               : 2022/01/19
 * Description        :
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for 
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#include "app_drv_fifo.h"
#include "CONFIG.h"
static __inline uint16_t fifo_length(app_drv_fifo_t *fifo)
{
    return (fifo->end - fifo->begin) & fifo->size_mask;
}
uint16_t app_drv_fifo_length(app_drv_fifo_t *fifo)
{
    return fifo_length(fifo);
}

app_drv_fifo_result_t
app_drv_fifo_init(app_drv_fifo_t *fifo, uint8_t *buffer, uint16_t buffer_size)
{
    if(buffer_size == 0)
    {
        return APP_DRV_FIFO_RESULT_LENGTH_ERROR;
    }
    if(0 != ((buffer_size) & (buffer_size - 1)))
    {
        return APP_DRV_FIFO_RESULT_LENGTH_ERROR;
    }
    fifo->begin = 0;
    fifo->end = 0;
    fifo->data = buffer;
    fifo->size = buffer_size;
    fifo->size_mask = buffer_size - 1;
    return APP_DRV_FIFO_RESULT_SUCCESS;
}

void app_drv_fifo_push(app_drv_fifo_t *fifo, uint8_t data)
{
    fifo->data[fifo->end & fifo->size_mask] = data;
    fifo->end++;
}

uint8_t app_drv_fifo_pop(app_drv_fifo_t *fifo)
{
    uint8_t data = fifo->data[fifo->begin & fifo->size_mask];
    fifo->begin++;
    return data;
}

void app_drv_fifo_flush(app_drv_fifo_t *fifo)
{
    fifo->begin = 0;
    fifo->end = 0;
}

bool app_drv_fifo_is_empty(app_drv_fifo_t *fifo)
{
    return (fifo->begin == fifo->end);
}

bool app_drv_fifo_is_full(app_drv_fifo_t *fifo)
{
    return (fifo_length(fifo) == fifo->size);
}

app_drv_fifo_result_t
app_drv_fifo_write(app_drv_fifo_t *fifo, uint8_t *data, uint16_t *p_write_length)
{
    if(fifo == NULL)
    {
        return APP_DRV_FIFO_RESULT_NULL;
    }
    if(p_write_length == NULL)
    {
        return APP_DRV_FIFO_RESULT_NULL;
    }
    //PRINT("fifo_length = %d\r\n",fifo_length(fifo));
    const uint16_t available_count = fifo->size - fifo_length(fifo);
    const uint16_t requested_len = (*p_write_length);
    uint16_t       index = 0;
    uint16_t       write_size = MIN(requested_len, available_count);
    //PRINT("available_count %d\r\n",available_count);
    // Check if the FIFO is FULL.
    if(available_count == 0)
    {
        return APP_DRV_FIFO_RESULT_NOT_MEM;
    }

    // Check if application has requested only the size.
    if(data == NULL)
    {
        return APP_DRV_FIFO_RESULT_SUCCESS;
    }

    for(index = 0; index < write_size; index++)
    {
        //push
        fifo->data[fifo->end & fifo->size_mask] = data[index];
        fifo->end++;
    }
    (*p_write_length) = write_size;
    return APP_DRV_FIFO_RESULT_SUCCESS;
}


app_drv_fifo_result_t app_drv_fifo_write_from_addr(app_drv_fifo_t *fifo, uint8_t *data, uint16_t len)
{
    if (fifo == NULL || data == NULL) {
        return APP_DRV_FIFO_RESULT_NULL;
    }

    uint16_t write_len = len;
    app_drv_fifo_result_t res = app_drv_fifo_write(fifo, data, &write_len);


    if (write_len < len) {

        return APP_DRV_FIFO_RESULT_NOT_MEM;
    }
    return res;
}
app_drv_fifo_result_t
app_drv_fifo_write_from_same_addr(app_drv_fifo_t *fifo, uint8_t *data, uint16_t write_length)
{
    if(fifo == NULL)
    {
        return APP_DRV_FIFO_RESULT_NULL;
    }
    //UART1_SendString(data, write_length);
    const uint16_t available_count = fifo->size - fifo_length(fifo);;
    const uint16_t requested_len = (write_length);
    uint16_t       index = 0;
    uint16_t       write_size = MIN(requested_len, available_count);
    uint8_t dat=0;
    // Check if the FIFO is FULL.
    if(available_count == 0)
    {
        return APP_DRV_FIFO_RESULT_NOT_MEM;
    }

    for(index = 0; index < write_size; index++)
    {
        //push
        dat=data[0];
        fifo->data[fifo->end & fifo->size_mask] = dat;
        UART1_SendByte(dat);

        fifo->end++;
    }
    return APP_DRV_FIFO_RESULT_SUCCESS;
}

app_drv_fifo_result_t
app_drv_fifo_read(app_drv_fifo_t *fifo, uint8_t *data, uint16_t *p_read_length)
{
    if(fifo == NULL)
    {
        return APP_DRV_FIFO_RESULT_NULL;
    }
    if(p_read_length == NULL)
    {
        return APP_DRV_FIFO_RESULT_NULL;
    }
    const uint16_t byte_count = fifo_length(fifo);
    const uint16_t requested_len = (*p_read_length);
    uint32_t       index = 0;
    uint32_t       read_size = MIN(requested_len, byte_count);

    if(byte_count == 0)
    {
        return APP_DRV_FIFO_RESULT_NOT_FOUND;
    }
    PRINT("read size = %d,byte_count = %d\r\n",read_size,byte_count);
    for(index = 0; index < read_size; index++)
    {
        //pop
        data[index] = fifo->data[fifo->begin & fifo->size_mask];
        fifo->begin++;
    }

    (*p_read_length) = read_size;
    return APP_DRV_FIFO_RESULT_SUCCESS;
}

app_drv_fifo_result_t
app_drv_fifo_read_to_same_addr(app_drv_fifo_t *fifo, uint8_t *data, uint16_t read_length)
{
    if(fifo == NULL)
    {
        return APP_DRV_FIFO_RESULT_NULL;
    }
    const uint16_t byte_count = fifo_length(fifo);
    const uint16_t requested_len = (read_length);
    uint32_t       index = 0;
    uint32_t       read_size = MIN(requested_len, byte_count);

    for(index = 0; index < read_size; index++)
    {
        //pop
        data[0] = fifo->data[fifo->begin & fifo->size_mask];
        fifo->begin++;
    }
    return APP_DRV_FIFO_RESULT_SUCCESS;
}



/* ==========================================================================
 * ★★ V1_0_2 修正：单帧最大长度的上限。
 * --------------------------------------------------------------------------
 * 帧长关系（本工程协议：AA EE | LEN(2B 大端) | CMD | DATA | SUM | 0A）：
 *     frame_total_len = 2(帧头) + 2(长度字段) + total_len + 2(SUM + 帧尾)
 *                     = total_len + 6
 * 调用方 Usart3_task.c 的 app_uart_process() 目标缓冲是 512 字节，
 * 而这里原来只挡 `total_len > 510` ⇒ frame_total_len 最大 516，
 * 下面那个拷贝循环会**越界写 4 字节**到调用方的缓冲区里（原来是栈上的
 * packet[512]！）—— 现场"CH584M 处理过多数据就卡住/跑飞"的来源之一。
 * 调用方那个 `if(pack_len < 512)` 判断在 read_pack() 返回**之后**才做，
 * 根本拦不住已经发生的越界拷贝，所以长度收口必须放在这里。
 * 上限取 APP_DRV_FIFO_PACK_MAX(512)：既保证不越过目标缓冲，
 * 也保证 frame_total_len 不超过 FIFO 自身的容量（511 字节可读），
 * 这样的帧永远不可能被拼"齐"，会在下面的 remain 判断处被安全丢弃。
 * ========================================================================== */
#ifndef APP_DRV_FIFO_PACK_MAX
#define APP_DRV_FIFO_PACK_MAX   512U
#endif

app_drv_fifo_result_t app_drv_fifo_read_pack(app_drv_fifo_t *fifo, uint8_t *data, uint16_t *pack_len)
{
    if (fifo == NULL || data == NULL || pack_len == NULL)
        return APP_DRV_FIFO_RESULT_NULL;

    *pack_len = 0;

    // 有效数据长度（环形缓冲区）
    uint16_t byte_count = (fifo->end - fifo->begin) & fifo->size_mask;
    if (byte_count == 0)
    {
        //PRINT("1111111111111\r\n");
        return APP_DRV_FIFO_RESULT_NOT_FOUND;
    }

    uint16_t offset = 0;
    // 寻找帧头 0xAA 0xEE
    while (offset < byte_count - 1)
    {
        uint8_t b1 = fifo->data[(fifo->begin + offset) & fifo->size_mask];
        uint8_t b2 = fifo->data[(fifo->begin + offset + 1) & fifo->size_mask];
        if (b1 == 0xAA && b2 == 0xEE)
            break;
        offset++;
    }


    //   未找到帧头 → 丢弃大部分数据，保留最后一个字节（可能为 AA 头）
      if (offset >= byte_count - 1)
      {
          fifo->begin = (fifo->begin + byte_count - 1) & fifo->size_mask;
        //  PRINT("2222222222\r\n");
          return APP_DRV_FIFO_RESULT_NOT_FOUND;
      }
    // 检查剩余数据是否足够读取长度字段（至少4字节：AA EE + 2字节长度）
    uint16_t remain = byte_count - offset;
    if (remain < 4)
    {
       // PRINT("33333333333\r\n");
        return APP_DRV_FIFO_RESULT_NOT_FOUND;
    }

    // 读取总长度（大端）
    uint16_t start = (fifo->begin + offset + 2) & fifo->size_mask;
    uint16_t total_len = (fifo->data[start] << 8) | fifo->data[(start + 1) & fifo->size_mask];

    // 保护：防止异常长度导致越界写调用方的目标缓冲区
    //   frame_total_len = total_len + 6 必须 <= APP_DRV_FIFO_PACK_MAX(512)
    //   ⇒ total_len <= 506。原来写的是 510（frame_total_len 最大 516），
    //     会越界写 4 字节 —— 见上面 APP_DRV_FIFO_PACK_MAX 的说明。
    if (total_len > (APP_DRV_FIFO_PACK_MAX - 6U)) {
        fifo->begin = (fifo->begin + offset + 1) & fifo->size_mask; // 跳过该 AA
        //PRINT("555555555555555\r\n");
        return APP_DRV_FIFO_RESULT_NOT_FOUND;
    }

    // 整包长度 = 帧头(2) + 长度字段(2) + total_len + 帧尾(1)
    uint16_t frame_total_len = 2 + 2 + total_len + 2;

    // 检查是否收齐整包
    if (remain < frame_total_len)
    {
       // PRINT("66666666666\r\n");
        return APP_DRV_FIFO_RESULT_NOT_FOUND;
    }
    // 检查帧尾
    uint16_t tail_idx = (fifo->begin + offset + frame_total_len - 1) & fifo->size_mask;
    if (fifo->data[tail_idx] != 0x0A) {
        fifo->begin = (fifo->begin + offset + 1) & fifo->size_mask; // 跳过该 AA
      //  PRINT("7777777777\r\n");
        return APP_DRV_FIFO_RESULT_NOT_FOUND;
    }

    // 校验和计算：从指令（帧头后第4字节）到校验和前一个字节
    uint8_t cal_sum = 0;
    uint16_t chk_start = (fifo->begin + offset + 4) & fifo->size_mask;
    uint16_t chk_end   = (fifo->begin + offset + frame_total_len - 3) & fifo->size_mask; // 校验和前一个字节

    uint16_t idx = chk_start;
    while (1) {
        cal_sum += fifo->data[idx];
        if (idx == chk_end) break;
        idx = (idx + 1) & fifo->size_mask;
    }

    uint8_t recv_sum = fifo->data[(fifo->begin + offset + frame_total_len - 2) & fifo->size_mask];
    if (cal_sum != recv_sum) {
        fifo->begin = (fifo->begin + offset + 1) & fifo->size_mask; // 跳过该 AA
    //    PRINT("88888888888\r\n");
        return APP_DRV_FIFO_RESULT_NOT_FOUND;
    }

    // 复制整包到输出缓冲区
    for (uint16_t i = 0; i < frame_total_len; i++)
    {
        data[i] = fifo->data[(fifo->begin + offset + i) & fifo->size_mask];
    }

    // 移动 begin 指针，弹出整包
    fifo->begin = (fifo->begin + offset + frame_total_len) & fifo->size_mask;
    *pack_len = frame_total_len;
    //PRINT("99999999999999\r\n");
    return APP_DRV_FIFO_RESULT_SUCCESS;
}
