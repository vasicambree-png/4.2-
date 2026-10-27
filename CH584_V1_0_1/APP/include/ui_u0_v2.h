#ifndef UI_U0_V2_H
#define UI_U0_V2_H

/* U0 contract V2, drawn in the existing 384x168 coordinate space. The
 * ui42_legacy callback projects geometry to the 400x600 driver canvas and
 * keeps glyphs native; source coordinates do not specify physical pixels.
 * This layer only renders: U0 still owns keys, pages and action commands. */
#define UI_U0_NO_DATA 0xFDFDu

static const char *const ui_u0_menu_names[8] = {
    "地址分区", "组网测试", "标定", "实时数据",
    "发送地址", "射频参数", "长度标定", "设备绑定"
};

static uint8_t ui_u0_signal_level(uint8_t magnitude)
{
    if (magnitude == 0u || magnitude == 0xFFu || magnitude > 130u) return 0u;
    if (magnitude > 110u) return 1u;
    if (magnitude > 95u) return 2u;
    if (magnitude > 85u) return 3u;
    return 4u;
}

static void ui_u0_right(uint16_t right, uint16_t y, const char *text, uint8_t size)
{
    uint16_t width = ui_text_width(text, size);
    ui_text_draw(width < right ? (uint16_t)(right - width) : 0u, y, text, size);
}

static void ui_u0_page(const char *title)
{
    u8g2_SetDrawColor(&u8g2, 1u);
    ui_main_cut_frame(1u, 1u, 382u, 166u, 8u);
    ui_text_draw(10u, 22u, title, 18u);
    u8g2_DrawHLine(&u8g2, 7u, 29u, 370u);
}

static void ui_u0_pair(uint16_t x, uint16_t right, uint16_t y,
                       const char *label, const char *value, uint8_t size)
{
    uint16_t lw = ui_text_width(label, size), vw = ui_text_width(value, size);
    uint16_t value_x = (uint16_t)(x + lw + 4u);
    ui_text_draw(x, y, label, size);
    if (right >= vw && right - vw >= value_x) value_x = (uint16_t)(right - vw);
    ui_text_draw(value_x, y, value, size);
}

static void ui_u0_focus(uint8_t selected, uint16_t x, uint16_t y,
                        uint16_t width, uint16_t height)
{
    u8g2_SetDrawColor(&u8g2, 1u);
    if (selected) ui_fast_DrawBox(&u8g2, x, y, width, height);
    else u8g2_DrawFrame(&u8g2, x, y, width, height);
    u8g2_SetDrawColor(&u8g2, selected ? 0u : 1u);
}

static void ui_u0_button(uint8_t slot, uint8_t option, const char *text)
{
    uint16_t x = slot ? 198u : 10u;
    ui_u0_focus((uint8_t)(Data_list1.rank3_addr == option), x, 140u, 175u, 22u);
    ui_text_draw((uint16_t)(x + 8u), 156u, text, 14u);
    u8g2_SetDrawColor(&u8g2, 1u);
}

#include "ui_home_physical.h"

static void ui_u0_menu(void)
{
    char value[40];
    uint8_t i, menu = Data_list1.rank2_addr;
    if (menu >= 8u) menu = 0u;
    u8g2_SetDrawColor(&u8g2, 1u);
    ui_main_cut_frame(1u, 1u, 382u, 166u, 8u);
    for (i = 0u; i < 8u; i++)
    {
        uint16_t y = (uint16_t)(19u + 20u * i);
        ui_u0_focus((uint8_t)(i == menu), 7u, (uint16_t)(y - 15u), 100u, 18u);
        ui_text_draw(13u, y, ui_u0_menu_names[i], 14u);
    }
    u8g2_SetDrawColor(&u8g2, 1u);
    u8g2_DrawVLine(&u8g2, 112u, 6u, 155u);
    ui_text_draw(123u, 23u, ui_u0_menu_names[menu], 18u);
    u8g2_DrawHLine(&u8g2, 120u, 29u, 255u);
    switch (menu)
    {
    case 0u:
        snprintf(value, sizeof(value), "%u", (unsigned)Data_list1.Menu_rank1.set_host_num);
        ui_u0_pair(124u, 369u, 59u, "本机地址", value, 14u);
        snprintf(value, sizeof(value), "%u", (unsigned)Data_list1.Menu_rank1.set_sub_num);
        ui_u0_pair(124u, 369u, 88u, "分区", value, 14u);
        break;
    case 1u:
        snprintf(value, sizeof(value), "%u", (unsigned)Data_list1.Menu_rank2.now_host_addr);
        ui_u0_pair(124u, 369u, 59u, "本机地址", value, 14u);
        ui_text_draw(124u, 88u, Data_list1.Menu_rank2.net_flag == NET_FLAG_IDLE ?
                     "等待组网" : Data_list1.Menu_rank2.net_flag == NET_FLAG_DONE ?
                     "组网完成" : "正在组网", 14u);
        break;
    case 2u:
        ui_text_draw(124u, 59u, Data_list1.Menu_rank3.pass_flag ? "已完成" : "未完成", 14u);
        ui_text_draw(124u, 88u, "标定值 / 当前AD", 14u);
        break;
    case 3u:
        ui_text_draw(124u, 59u, "20通道实时数据", 14u);
        snprintf(value, sizeof(value), "%u", (unsigned)Data_list1.Menu_rank4.kaer_time);
        ui_u0_pair(124u, 369u, 88u, "卡尔曼时间", value, 14u);
        break;
    case 4u:
        snprintf(value, sizeof(value), "%u / %u / %u",
                 (unsigned)Data_list1.Menu_rank5.new_send_addr[0],
                 (unsigned)Data_list1.Menu_rank5.new_send_addr[1],
                 (unsigned)Data_list1.Menu_rank5.new_send_addr[2]);
        ui_text_draw(124u, 59u, "新发送地址", 14u);
        ui_text_draw(124u, 88u, value, 14u);
        break;
    case 5u:
        snprintf(value, sizeof(value), "%d dB", (int)(int16_t)Data_list1.Menu_rank6.power);
        ui_u0_pair(124u, 369u, 59u, "功率", value, 14u);
        snprintf(value, sizeof(value), "%u s", (unsigned)Data_list1.Menu_rank6.time_light);
        ui_u0_pair(124u, 369u, 88u, "亮屏时间", value, 14u);
        ui_text_draw(124u, 117u, Data_list1.Menu_rank6.state ? "通信开机" : "通信不开机", 14u);
        break;
    case 6u:
        snprintf(value, sizeof(value), "%u", (unsigned)Data_list1.Menu_rank7.old_len);
        ui_u0_pair(124u, 369u, 59u, "旧标定", value, 14u);
        break;
    case 7u:
        snprintf(value, sizeof(value), "%u台", (unsigned)g_binding_count);
        ui_u0_pair(124u, 369u, 59u, "已绑定设备", value, 14u);
        ui_text_draw(124u, 88u, "保存 / 解绑 / 恢复出厂", 11u);
        break;
    }
    ui_text_draw(124u, 153u, "K1 选择   K2 确认   K3 返回", 11u);
}

static void ui_u0_subpage(uint8_t menu)
{
    char label[32], value[48];
    uint8_t i, focus = Data_list1.rank3_addr;
    ui_u0_page(ui_u0_menu_names[menu]);
    switch (menu)
    {
    case 0u:
        if (focus <= 3u) u8g2_DrawFrame(&u8g2, focus & 1u ? 220u : 9u,
            focus <= 1u ? 44u : 79u, focus & 1u ? 155u : 200u, 23u);
        snprintf(value, sizeof(value), "%u", (unsigned)Data_list1.Menu_rank1.set_sub_num);
        ui_u0_pair(15u, 360u, 61u, "分区", value, 16u);
        snprintf(value, sizeof(value), "%u", (unsigned)Data_list1.Menu_rank1.set_host_num);
        ui_u0_pair(15u, 360u, 96u, "本机地址", value, 16u);
        ui_u0_button(0u, 4u, "保存并返回");
        ui_u0_button(1u, 5u, "不保存返回");
        break;
    case 1u:
        snprintf(value, sizeof(value), "%u", (unsigned)Data_list1.Menu_rank2.now_host_addr);
        ui_u0_pair(15u, 365u, 47u, "本机地址", value, 14u);
        for (i = 0u; i < 3u; i++)
        {
            uint16_t rssi = Data_list1.Menu_rank2.rssi[i];
            uint16_t y = (uint16_t)(72u + 24u * i);
            uint8_t current = (uint8_t)(Data_list1.Menu_rank2.net_flag != NET_FLAG_IDLE &&
                Data_list1.Menu_rank2.net_flag != NET_FLAG_DONE && Data_list1.Menu_rank2.cur_index == i);
            if (current) u8g2_DrawFrame(&u8g2, 9u, (uint16_t)(y - 16u), 366u, 20u);
            snprintf(label, sizeof(label), "地址%u", (unsigned)Data_list1.Menu_rank2.addr[i]);
            if (rssi == 0xFFu) snprintf(value, sizeof(value), "次数%u  信号--", (unsigned)Data_list1.Menu_rank2.cnt[i]);
            else snprintf(value, sizeof(value), "次数%u  信号%u", (unsigned)Data_list1.Menu_rank2.cnt[i], (unsigned)rssi);
            ui_u0_pair(15u, 366u, y, label, value, 14u);
        }
        if (Data_list1.Menu_rank2.net_flag != NET_FLAG_IDLE &&
            Data_list1.Menu_rank2.net_flag != NET_FLAG_DONE)
            ui_text_draw(15u, 153u, "正在组网", 14u);
        else
        {
            ui_u0_button(0u, 0u, Data_list1.Menu_rank2.net_flag == NET_FLAG_DONE ? "重测组网" : "开始组网");
            ui_u0_button(1u, 1u, "返回");
        }
        break;
    case 2u:
        snprintf(value, sizeof(value), "%u/%u", (unsigned)Data_list1.Menu_rank3.pass_buf[0],
                 (unsigned)Data_list1.Menu_rank3.pass_buf[1]);
        ui_u0_pair(15u, 366u, 49u, "本机/分区", value, 14u);
        for (i = 0u; i < 3u; i++)
        {
            uint16_t y = (uint16_t)(73u + 24u * i);
            if (focus == i) u8g2_DrawFrame(&u8g2, 9u, (uint16_t)(y - 16u), 366u, 20u);
            snprintf(label, sizeof(label), "标定%u", (unsigned)(i + 1u));
            snprintf(value, sizeof(value), "%u / AD%u  %s", (unsigned)Data_list1.Menu_rank3.biaoding_ad[i][0],
                     (unsigned)Data_list1.Menu_rank3.biaoding_ad[i][1],
                     Data_list1.Menu_rank3.biaoding_flag[i] ? "成功" : "未标");
            ui_u0_pair(15u, 366u, y, label, value, 14u);
        }
        snprintf(value, sizeof(value), "%d/%u  %s", (int)(int8_t)Data_list1.Menu_rank3.pass_buf[2],
                 (unsigned)Data_list1.Menu_rank3.pass_buf[3], Data_list1.Menu_rank3.pass_flag ? "已完成" : "未完成");
        ui_u0_pair(15u, 366u, 153u, "功率/版本", value, 14u);
        break;
    case 3u:
        for (i = 0u; i < MAX_CH_NUM; i++)
        {
            uint16_t x = i < 10u ? 15u : 204u, y = (uint16_t)(43u + 11u * (i % 10u));
            uint16_t raw = Data_list1.Menu_rank4.shishi_buf[i];
            snprintf(label, sizeof(label), "%u", (unsigned)(i + 1u));
            if (raw == UI_U0_NO_DATA) snprintf(value, sizeof(value), "--");
            /* This V2 page is the U0 raw-value view, without channel types or
             * units. Engineering scaling belongs to the typed home page. */
            else snprintf(value, sizeof(value), "%u", (unsigned)raw);
            ui_u0_pair(x, (uint16_t)(x + 164u), y, label, value, 11u);
        }
        snprintf(value, sizeof(value), "%u", (unsigned)Data_list1.Menu_rank4.kaer_time);
        ui_u0_pair(15u, 366u, 158u, "卡尔曼时间", value, 11u);
        break;
    case 4u:
        for (i = 0u; i < 3u; i++)
        {
            uint16_t y = (uint16_t)(58u + 29u * i);
            if (focus == i) u8g2_DrawFrame(&u8g2, 9u, (uint16_t)(y - 17u), 366u, 22u);
            snprintf(label, sizeof(label), "地址%u", (unsigned)(i + 1u));
            snprintf(value, sizeof(value), "%u --> %u", (unsigned)Data_list1.Menu_rank5.old_send_addr[i],
                     (unsigned)Data_list1.Menu_rank5.new_send_addr[i]);
            ui_u0_pair(15u, 366u, y, label, value, 16u);
        }
        ui_u0_button(0u, 3u, "保存并返回");
        ui_u0_button(1u, 4u, "不保存返回");
        break;
    case 5u:
        if (focus <= 5u) u8g2_DrawFrame(&u8g2, focus & 1u ? 220u : 9u,
            (uint16_t)(34u + 29u * (focus / 2u)), focus & 1u ? 155u : 200u, 22u);
        snprintf(value, sizeof(value), "%d dB", (int)(int16_t)Data_list1.Menu_rank6.power);
        ui_u0_pair(15u, 366u, 51u, "功率", value, 16u);
        snprintf(value, sizeof(value), "%u s", (unsigned)Data_list1.Menu_rank6.time_light);
        ui_u0_pair(15u, 366u, 80u, "亮屏时间", value, 16u);
        ui_text_draw(15u, 109u, Data_list1.Menu_rank6.state ? "通信开机" : "通信不开机", 16u);
        ui_u0_button(0u, 6u, "保存并返回");
        ui_u0_button(1u, 7u, "不保存返回");
        break;
    case 6u:
        for (i = 0u; i < 3u; i++)
        {
            uint16_t y = (uint16_t)(56u + 24u * i), raw = Data_list1.Menu_rank7.len_value[i];
            if (focus == i) u8g2_DrawFrame(&u8g2, 9u, (uint16_t)(y - 16u), 366u, 20u);
            snprintf(label, sizeof(label), "长度%u AD%u", (unsigned)(i + 1u), (unsigned)Data_list1.Menu_rank7.ad_value[i]);
            if (raw == UI_U0_NO_DATA) snprintf(value, sizeof(value), "--");
            else snprintf(value, sizeof(value), "%u", (unsigned)raw);
            ui_u0_pair(15u, 366u, y, label, value, 14u);
        }
        snprintf(value, sizeof(value), "%u", (unsigned)Data_list1.Menu_rank7.old_len);
        ui_u0_pair(15u, 366u, 130u, "旧标定", value, 14u);
        if (Data_list1.Menu_rank7.new_len == UI_U0_NO_DATA) snprintf(value, sizeof(value), "--");
        else snprintf(value, sizeof(value), "%u", (unsigned)Data_list1.Menu_rank7.new_len);
        ui_u0_pair(15u, 366u, 154u, "新读数", value, 14u);
        break;
    case 7u:
    {
        static const char *const actions[4] = {"保存绑定", "解绑", "恢复出厂", "返回"};
        for (i = 0u; i < 4u; i++)
        {
            uint16_t y = (uint16_t)(35u + 24u * i);
            ui_u0_focus((uint8_t)(focus == i), 20u, y, 343u, 20u);
            ui_text_draw(31u, (uint16_t)(y + 15u), actions[i], 14u);
        }
        u8g2_SetDrawColor(&u8g2, 1u);
        snprintf(value, sizeof(value), "已绑定:%u台  K1选择 K2确认 K3返回", (unsigned)g_binding_count);
        ui_text_draw(15u, 144u, value, 11u);
        ui_text_draw(15u, 158u, "进入本页自动扫描，保存后退出再进入绑定", 11u);
        break;
    }
    }
}

static void ui_u0_v2_draw(uint8_t rank, uint8_t menu)
{
    if (menu >= 8u) menu = 0u;
    if (rank == 1u) ui_u0_home();
    else if (rank == 2u) ui_u0_menu();
    else if (rank == 3u) ui_u0_subpage(menu);
    else if (rank == 4u) new_return();
    u8g2_SetDrawColor(&u8g2, 1u);
}

#endif
