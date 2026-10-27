#ifndef UI_MENU_42_H
#define UI_MENU_42_H

#include "ui_channel_icons.h"

/* 4.2-inch ST7306 physical (400x300) secondary and tertiary pages.
 * All Chinese text uses Microsoft YaHei Bold (msyhbd.ttc) in 18u, 16u, and 14u
 * where character coverage is 100% complete (no missing glyphs or '?').
 * Version in Menu 7 is removed per user request (already shown on home page).
 * The STM32 protocol, keys, and rank 1 (home page) remain untouched. */

static void ui_menu_42_text_right(uint16_t right, uint16_t baseline, const char *text, uint8_t size)
{
    uint16_t width = ui_text_width(text, size);
    ui_text_draw(width < right ? (uint16_t)(right - width) : 0u, baseline, text, size);
}

static void ui_menu_42_control(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                              const char *text, uint8_t selected, uint8_t centered, uint8_t size)
{
    uint16_t tw = ui_text_width(text, size);
    uint16_t tx;
    if (ui_render_layer == UI_LAYER_STATIC) return;
    u8g2_SetDrawColor(&u8g2, 0);
    if (selected) ui_fast_DrawBox(&u8g2, x, y, w, h);
    else u8g2_DrawFrame(&u8g2, x, y, w, h);
    u8g2_SetDrawColor(&u8g2, selected ? 1u : 0u);
    tx = centered ? (uint16_t)(x + (w > tw ? (w - tw) / 2u : 0u)) : (uint16_t)(x + 8u);
    ui_text_draw(tx, (uint16_t)(y + (h > size ? (h - size) / 2u : 0u) + size - 1u), text, size);
}

static void ui_menu_42_button(uint8_t right, const char *text, uint8_t selected)
{
    uint16_t bx = right ? 266u : 132u;
    ui_menu_42_control(bx, 246u, 124u, 36u, text, selected, 1u, 16u);
}

static void ui_menu_42_field(uint16_t y, const char *label, const char *value,
                             uint8_t label_selected, uint8_t value_selected)
{
    uint16_t h = 56u;
    if (ui_render_layer == UI_LAYER_STATIC) return;
    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_DrawFrame(&u8g2, 132u, y, 258u, h);
    if (label_selected) ui_fast_DrawBox(&u8g2, 134u, (uint16_t)(y + 2u), 146u, (uint16_t)(h - 4u));
    if (value_selected) ui_fast_DrawBox(&u8g2, 282u, (uint16_t)(y + 2u), 106u, (uint16_t)(h - 4u));
    u8g2_SetDrawColor(&u8g2, label_selected ? 1u : 0u);
    ui_text_draw(140u, (uint16_t)(y + (h + 18u) / 2u - 2u), label, 18u);
    u8g2_SetDrawColor(&u8g2, value_selected ? 1u : 0u);
    ui_text_draw(296u, (uint16_t)(y + (h + 18u) / 2u - 2u), value, 18u);
    u8g2_SetDrawColor(&u8g2, 0);
}

static void ui_menu_42_header(uint8_t menu)
{
    if (ui_render_layer == UI_LAYER_DYNAMIC) return;
    if (menu >= MAIN_LEN) menu = 0u;
    u8g2_SetDrawColor(&u8g2, 1);
    ui_fast_DrawBox(&u8g2, 124u, 4u, 272u, 292u);
    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_DrawRFrame(&u8g2, 126u, 6u, 268u, 288u, 4u);
    ui_fast_DrawXBMP(&u8g2, 134u, 12u, 29u, 27u, ui_menu_title_icons[menu]);
    ui_text_draw(174u, 34u, Menu_List[menu].name, 18u);
    u8g2_DrawHLine(&u8g2, 128u, 46u, 262u);
}

static void addr_Control_42(void)
{
    char value[20];
    sprintf(value, "%d", Data_list1.Menu_rank1.set_sub_num);
    ui_menu_42_field(76u, "分站号:", value, ui_menu_selected(0u), ui_menu_selected(1u));
    sprintf(value, "%d", Data_list1.Menu_rank1.set_host_num);
    ui_menu_42_field(158u, "本机地址:", value, ui_menu_selected(2u), ui_menu_selected(3u));
    ui_menu_42_button(0u, "保存并返回", ui_menu_selected(4u));
    ui_menu_42_button(1u, "不保存返回", ui_menu_selected(5u));
}

static void networking_Control1_42(void)
{
    uint16_t dy = 32u;
    UI_STATIC {
        ui_text_draw(136u, (uint16_t)(82u + dy), "组网", 18u);
        u8g2_DrawFrame(&u8g2, 142u, (uint16_t)(110u + dy), 38u, 26u);
        u8g2_DrawFrame(&u8g2, 146u, (uint16_t)(114u + dy), 30u, 18u);
        u8g2_DrawHLine(&u8g2, 180u, (uint16_t)(123u + dy), 48u);
        ui_fast_DrawXBMP(&u8g2, 230u, (uint16_t)(108u + dy), 29u, 27u, ui_menu_title_icons[1]);
        u8g2_DrawHLine(&u8g2, 260u, (uint16_t)(123u + dy), 48u);
        u8g2_DrawFrame(&u8g2, 310u, (uint16_t)(110u + dy), 38u, 26u);
    }
    ui_menu_42_button(0u, "开始组网", ui_menu_selected(0u));
    ui_menu_42_button(1u, "返回", ui_menu_selected(1u));
}

static void networking_Result_Page_42(void)
{
    char buf[36];
    uint8_t i;
    uint16_t flag = Data_list1.Menu_rank2.net_flag;
    uint8_t running = (uint8_t)(flag != NET_FLAG_IDLE && flag != NET_FLAG_DONE);

    u8g2_SetDrawColor(&u8g2, 1);
    ui_fast_DrawBox(&u8g2, 2u, 2u, 396u, 296u);
    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_DrawRFrame(&u8g2, 4u, 4u, 392u, 292u, 4u);

    ui_fast_DrawXBMP(&u8g2, 14u, 12u, 13u, 13u, ui_menu_nav_icons[1]);
    ui_text_draw(34u, 28u, "组网测试", 18u);

    sprintf(buf, "本机地址:%d", (int)Data_list1.Menu_rank2.now_host_addr);
    ui_menu_42_text_right(384u, 28u, buf, 16u);
    u8g2_DrawHLine(&u8g2, 8u, 44u, 384u);

    ui_text_draw(28u, 74u, "测试地址", 16u);
    ui_text_draw(164u, 74u, "测试次数", 16u);
    ui_text_draw(300u, 74u, "成功比率", 16u);
    u8g2_DrawHLine(&u8g2, 12u, 84u, 376u);

    for (i = 0u; i < 3u; i++)
    {
        uint16_t y = (uint16_t)(124u + 46u * i);
        uint16_t cnt = Data_list1.Menu_rank2.cnt[i];
        uint16_t succ = Data_list1.Menu_rank2.succ[i];
        uint16_t pct = cnt ? (uint16_t)(((uint32_t)succ * 100u + cnt / 2u) / cnt) : 0u;
        uint8_t current = (uint8_t)(running && Data_list1.Menu_rank2.cur_index == i);
        if (current) u8g2_DrawFrame(&u8g2, 14u, (uint16_t)(y - 24u), 372u, 38u);
        if (Data_list1.Menu_rank2.addr[i] == 0u) strcpy(buf, "中继");
        else if (Data_list1.Menu_rank2.addr[i] == 121u) strcpy(buf, "无");
        else if (Data_list1.Menu_rank2.addr[i] == 122u) strcpy(buf, "分站");
        else sprintf(buf, "%d号", (int)Data_list1.Menu_rank2.addr[i]);
        ui_text_draw(28u, y, buf, 18u);
        sprintf(buf, "%d/10", (int)cnt);
        ui_text_draw(164u, y, buf, 18u);
        sprintf(buf, "%d%%", (int)pct);
        ui_text_draw(300u, y, buf, 18u);
    }

    ui_menu_42_control(14u, 252u, 180u, 36u,
                       running ? "正在组网" : (flag == NET_FLAG_DONE ? "重测组网" : "开始组网"),
                       (uint8_t)(!running && ui_menu_selected(0u)), 1u, 16u);
    ui_menu_42_control(206u, 252u, 180u, 36u, "返回",
                       (uint8_t)(!running && ui_menu_selected(1u)), 1u, 16u);
}

static void networking_Control_42(void)
{
    if (Data_list1.menu_rank != 3u || Data_list1.Menu_rank2.net_flag == NET_FLAG_IDLE)
    {
        networking_Control1_42();
        return;
    }
    networking_Result_Page_42();
}

/* Keep all twenty positions visible: left 1..10, right 11..20.
 * Device count only controls names; unused positions display "--". */
static void ui_menu_42_binding_rows(uint8_t n)
{
    uint8_t i;
    char buf[16];
    if (n > 20u) n = 20u;
    u8g2_DrawVLine(&u8g2, 200u, 46u, 202u);
    for (i = 0u; i < 20u; i++)
    {
        uint16_t x = i < 10u ? 16u : 210u;
        uint16_t y = (uint16_t)(66u + (i % 10u) * 19u);
        sprintf(buf, "%d:", (int)(i + 1u));
        ui_text_draw(x, y, buf, 14u);
        if (i < n && g_binding_list[i].name[0] != '\0')
            ui_draw_name_aligned((uint16_t)(x + 28u), y, g_binding_list[i].name,
                                 150u, 16u, 0u);
        else
            ui_text_draw((uint16_t)(x + 28u), y, "--", 16u);
        if (i % 10u != 9u)
            u8g2_DrawHLine(&u8g2, x, (uint16_t)(y + 3u), 176u);
    }
}

static void binding_device_list_page_42(uint8_t rank3)
{
    char buf[48];
    u8g2_SetDrawColor(&u8g2, 1);
    ui_fast_DrawBox(&u8g2, 2u, 2u, 396u, 296u);
    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_DrawRFrame(&u8g2, 4u, 4u, 392u, 292u, 4u);

    ui_fast_DrawXBMP(&u8g2, 14u, 12u, UI_CHANNEL_ICON_SIZE, UI_CHANNEL_ICON_SIZE, ui_channel_icon_2);
    ui_text_draw(40u, 28u, "设备绑定", 18u);

    sprintf(buf, "总设备数:%d台", (int)g_binding_count);
    ui_text_draw(120u, 28u, buf, 16u);
    u8g2_DrawHLine(&u8g2, 8u, 44u, 384u);

    ui_menu_42_binding_rows(g_binding_count);

    ui_menu_42_control(14u, 252u, 180u, 36u, "一键解绑", (uint8_t)(rank3 == 0u), 1u, 16u);
    ui_menu_42_control(206u, 252u, 180u, 36u, "返回", (uint8_t)(rank3 != 0u), 1u, 16u);
}

static void binding_scan_page_42(uint8_t rank3)
{
    char buf[48];
    u8g2_SetDrawColor(&u8g2, 1);
    ui_fast_DrawBox(&u8g2, 2u, 2u, 396u, 296u);
    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_DrawRFrame(&u8g2, 4u, 4u, 392u, 292u, 4u);

    ui_fast_DrawXBMP(&u8g2, 14u, 12u, UI_CHANNEL_ICON_SIZE, UI_CHANNEL_ICON_SIZE, ui_channel_icon_2);
    ui_text_draw(40u, 28u, "设备绑定", 18u);

    sprintf(buf, "已绑定设备:%u台", (unsigned int)g_binding_count);
    ui_text_draw(120u, 28u, buf, 16u);
    u8g2_DrawHLine(&u8g2, 8u, 44u, 384u);

    ui_menu_42_binding_rows(g_binding_count);

    ui_menu_42_control(14u, 252u, 180u, 36u, "保存目前设备", (uint8_t)(rank3 == 0u), 1u, 16u);
    ui_menu_42_control(206u, 252u, 180u, 36u, "返回", (uint8_t)(rank3 != 0u), 1u, 16u);
}

static void binding_Control_42(void)
{
    uint8_t re = Data_list1.UI_main.re_flag;
    uint8_t rank3 = Data_list1.rank3_addr;
    char buf[48];
    if (re == 2 && Data_list1.menu_rank == 3u) { binding_scan_page_42(rank3); return; }
    if (re == 1 && Data_list1.menu_rank == 3u) { binding_device_list_page_42(rank3); return; }
    sprintf(buf, "已绑定设备:%u台", (unsigned int)g_binding_count);
    ui_menu_42_control(132u, 62u, 258u, 48u, buf, ui_menu_selected(0u), 0u, 18u);
    ui_menu_42_control(132u, 121u, 258u, 48u, "一键解绑", ui_menu_selected(1u), 0u, 18u);
    ui_menu_42_control(132u, 180u, 258u, 48u, "绑定设备", ui_menu_selected(2u), 0u, 18u);
    ui_menu_42_button(1u, "返回", ui_menu_selected(3u));
}

static void install_ch_page_42(uint8_t mode)
{
    uint8_t ch;
    char buf[36];
    u8g2_SetDrawColor(&u8g2, 1);
    ui_fast_DrawBox(&u8g2, 2u, 2u, 396u, 296u);
    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_DrawRFrame(&u8g2, 4u, 4u, 392u, 292u, 4u);

    ui_fast_DrawXBMP(&u8g2, 14u, 12u, UI_CHANNEL_ICON_SIZE, UI_CHANNEL_ICON_SIZE, ui_channel_icon_3);
    ui_text_draw(40u, 28u, mode == 3 ? "设备信号" : "设备电压", 18u);
    u8g2_DrawHLine(&u8g2, 8u, 44u, 384u);

    u8g2_DrawVLine(&u8g2, 200u, 46u, 202u);

    for (ch = 0u; ch < MAX_CH_NUM; ch++)
    {
        uint16_t x = ch < 10u ? 16u : 212u;
        uint16_t y = (uint16_t)(66u + (ch % 10u) * 19u);
        device_t *d = &CH_com_buf[ch];
        sprintf(buf, "%2d:", (int)(ch + 1u));
        ui_text_draw(x, y, buf, 16u);

        if (!(d->valid && d->data_re_flag)) strcpy(buf, "--");
        else if (mode == 3) sprintf(buf, "%d dBm", (int)(int8_t)d->rssi);
        else sprintf(buf, "%d.%d V", (int)(d->voltage / 10), (int)(d->voltage % 10));
        ui_text_draw((uint16_t)(x + 46u), y, buf, 16u);

        if (ch % 10u != 9u)
            u8g2_DrawHLine(&u8g2, x, (uint16_t)(y + 3u), 176u);
    }

    ui_menu_42_control(138u, 252u, 124u, 36u, "返回", 1u, 1u, 16u);
}

static void install_Control_42(void)
{
    uint8_t re = Data_list1.UI_main.re_flag;
    if (re == 3 && Data_list1.menu_rank == 3u) { install_ch_page_42(3); return; }
    if (re == 4 && Data_list1.menu_rank == 3u) { install_ch_page_42(4); return; }
    ui_menu_42_control(132u, 72u, 258u, 66u, "设备信号", ui_menu_selected(0u), 0u, 18u);
    ui_menu_42_control(132u, 158u, 258u, 66u, "设备电压", ui_menu_selected(1u), 0u, 18u);
    ui_menu_42_button(1u, "返回", ui_menu_selected(2u));
}

static void uploading_Control_42(void)
{
    uint8_t i;
    char label[16], old_value[20], new_value[20];
    UI_STATIC {
        ui_text_draw(206u, 70u, "已存地址", 16u);
        ui_text_draw(308u, 70u, "更改为", 16u);
    }
    for (i = 0u; i < 3u; i++)
    {
        uint16_t y = (uint16_t)(88u + (52u) * i);
        uint16_t old = Data_list1.Menu_rank5.old_send_addr[i];
        uint16_t next = Data_list1.Menu_rank5.new_send_addr[i];
        sprintf(label, "上传%d:", (int)(i + 1u));
        if (old == 0u) strcpy(old_value, "中继");
        else if (old == 121u) strcpy(old_value, "无");
        else if (old == 122u) strcpy(old_value, "分站");
        else sprintf(old_value, "%d号", old);

        if (next == 0u) strcpy(new_value, "中继");
        else if (next == 121u) strcpy(new_value, "无");
        else if (next == 122u) strcpy(new_value, "分站");
        else sprintf(new_value, "%d号", next);

        UI_STATIC { ui_text_draw(134u, (uint16_t)(y + (28u)), label, 16u); }
        ui_menu_42_control(200u, y, 92u, 42u, old_value, 0u, 1u, 16u);
        ui_menu_42_control(298u, y, 92u, 42u, new_value, ui_menu_selected(i), 1u, 16u);
    }
    ui_menu_42_button(0u, "保存并返回", ui_menu_selected(3u));
    ui_menu_42_button(1u, "不保存返回", ui_menu_selected(4u));
}

/* This page reserves the right-hand column for the bulb illustration. */
static void ui_menu_42_other_field(uint16_t y, const char *label, const char *value,
                                   uint8_t label_selected, uint8_t value_selected)
{
    uint16_t h = 50u;
    if (ui_render_layer == UI_LAYER_STATIC) return;
    u8g2_SetDrawColor(&u8g2, 0u);
    u8g2_DrawFrame(&u8g2, 132u, y, 194u, h);
    if (label_selected) ui_fast_DrawBox(&u8g2, 134u, (uint16_t)(y + 2u), 88u, (uint16_t)(h - 4u));
    if (value_selected) ui_fast_DrawBox(&u8g2, 224u, (uint16_t)(y + 2u), 100u, (uint16_t)(h - 4u));
    u8g2_SetDrawColor(&u8g2, label_selected ? 1u : 0u);
    ui_text_draw(140u, (uint16_t)(y + (h + 16u) / 2u - 2u), label, 16u);
    u8g2_SetDrawColor(&u8g2, value_selected ? 1u : 0u);
    ui_text_draw(232u, (uint16_t)(y + (h + 16u) / 2u - 2u), value, 16u);
    u8g2_SetDrawColor(&u8g2, 0u);
}

static void Other_Settings_Control_42(void)
{
    char value[20];
    sprintf(value, "%d db", Data_list1.Menu_rank6.power);
    ui_menu_42_other_field(64u, "通信功率:", value, ui_menu_selected(0u), ui_menu_selected(1u));
    sprintf(value, "%d 秒", Data_list1.Menu_rank6.time_light);
    ui_menu_42_other_field(120u, "亮屏时间:", value, ui_menu_selected(2u), ui_menu_selected(3u));
    ui_menu_42_other_field(176u, "通信状态:", Data_list1.Menu_rank6.state == 1 ? "开机" : "不开机",
                     ui_menu_selected(4u), ui_menu_selected(5u));
    UI_STATIC { ui_fast_DrawXBMP(&u8g2, 336u, 116u, 46u, 58u, ui_menu_detail_other); }
    ui_menu_42_button(0u, "保存并返回", ui_menu_selected(6u));
    ui_menu_42_button(1u, "不保存返回", ui_menu_selected(7u));
}

static void summary_name_page_42(uint8_t rank3)
{
    uint8_t ch;
    char buf[36];
    u8g2_SetDrawColor(&u8g2, 1);
    ui_fast_DrawBox(&u8g2, 2u, 2u, 396u, 296u);
    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_DrawRFrame(&u8g2, 4u, 4u, 392u, 292u, 4u);

    ui_fast_DrawXBMP(&u8g2, 14u, 12u, UI_CHANNEL_ICON_SIZE, UI_CHANNEL_ICON_SIZE, ui_channel_icon_6);
    ui_text_draw(40u, 28u, "已绑定设备名称", 18u);
    u8g2_DrawHLine(&u8g2, 8u, 44u, 384u);

    u8g2_DrawVLine(&u8g2, 200u, 46u, 202u);

    for (ch = 0u; ch < MAX_CH_NUM; ch++)
    {
        uint16_t x = ch < 10u ? 16u : 212u;
        uint16_t y = (uint16_t)(66u + (ch % 10u) * 19u);
        device_t *d = &CH_com_buf[ch];
        sprintf(buf, "%2d:", (int)(ch + 1u));
        ui_text_draw(x, y, buf, 14u);
        if (d->valid && d->name[0] != '\0')
            ui_draw_name_aligned((uint16_t)(x + 32u), y, d->name, 150u, 16u, 0u);
        else
            ui_text_draw((uint16_t)(x + 46u), y, "--", 16u);

        if (ch % 10u != 9u)
            u8g2_DrawHLine(&u8g2, x, (uint16_t)(y + 3u), 176u);
    }
    ui_menu_42_control(138u, 252u, 124u, 36u, "返回", 1u, 1u, 16u);
}

static void summary_factory_page_42(uint8_t rank3)
{
    u8g2_SetDrawColor(&u8g2, 1);
    ui_fast_DrawBox(&u8g2, 2u, 2u, 396u, 296u);
    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_DrawRFrame(&u8g2, 4u, 4u, 392u, 292u, 4u);

    ui_fast_DrawXBMP(&u8g2, 14u, 12u, 13u, 13u, ui_menu_nav_icons[6]);
    ui_text_draw(34u, 28u, "恢复出厂设置", 18u);
    u8g2_DrawHLine(&u8g2, 8u, 44u, 384u);

    u8g2_DrawFrame(&u8g2, 26u, 68u, 348u, 214u);
    ui_text_draw(95u, 144u, "确认恢复出厂设置?", 18u);
    ui_menu_42_control(46u, 216u, 144u, 48u, "确认", (uint8_t)(rank3 == 0u), 1u, 16u);
    ui_menu_42_control(210u, 216u, 144u, 48u, "返回", (uint8_t)(rank3 != 0u), 1u, 16u);
}

static void summary_Control_42(void)
{
    uint8_t re = Data_list1.UI_main.re_flag;
    uint8_t rank3 = Data_list1.rank3_addr;
    char buf[48];
    if (re == 5 && Data_list1.menu_rank == 3u) { summary_name_page_42(rank3); return; }
    if (re == 6 && Data_list1.menu_rank == 3u) { summary_factory_page_42(rank3); return; }
    sprintf(buf, "查看已绑定的设备:%u台", (unsigned int)g_binding_count);
    ui_menu_42_control(132u, 56u, 258u, 38u, buf, ui_menu_selected(0u), 0u, 16u);
    sprintf(buf, "蓝牙名称错误警报:%d", (int)g_name_err_count);
    ui_menu_42_control(132u, 102u, 258u, 38u, buf, ui_menu_selected(1u), 0u, 16u);
    sprintf(buf, "蓝牙电压异常警报:%d", (int)g_volt_err_count);
    ui_menu_42_control(132u, 148u, 258u, 38u, buf, ui_menu_selected(2u), 0u, 16u);
    ui_menu_42_control(132u, 194u, 258u, 38u, "恢复出厂", ui_menu_selected(3u), 0u, 16u);
    ui_menu_42_button(1u, "返回", ui_menu_selected(4u));
}

static void return_main_Control_42(void)
{
    uint16_t slogan_x = (uint16_t)(127u + (266u - UI_RETURN_SLOGAN_WIDTH) / 2u);
    uint16_t row;

    UI_STATIC {
        u8g2_SetDrawColor(&u8g2, 1u);
        ui_fast_DrawBox(&u8g2, 124u, 4u, 272u, 292u);
        u8g2_SetDrawColor(&u8g2, 0u);
        /* Fill the right pane by expanding only the artwork's blank bands.
         * Text and icons retain their native aspect ratio and pixel size. */
        for (row = 0u; row < 284u; row++)
        {
            uint16_t source_row;
            if (row < 42u) source_row = row;
            else if (row < 78u) source_row = 41u;
            else if (row < 144u) source_row = (uint16_t)(row - 36u);
            else if (row < 228u) source_row = 109u;
            else source_row = (uint16_t)(row - 118u);
            ui_fast_DrawXBMP(&u8g2, 127u, (uint16_t)(8u + row), 266u, 1u,
                             &ui_menu_return_page[source_row * 34u]);
        }
        /* Cleanly erase the bitmap's bottom fuzzy text block */
        u8g2_SetDrawColor(&u8g2, 1u);
        ui_fast_DrawBox(&u8g2, 133u, 260u, 254u, 26u);
        u8g2_SetDrawColor(&u8g2, 0u);
        /* Draw the crisp dedicated slogan artwork exactly once */
        ui_fast_DrawXBMP(&u8g2, slogan_x, 260u,
                         UI_RETURN_SLOGAN_WIDTH, UI_RETURN_SLOGAN_HEIGHT,
                         ui_menu_return_slogan);
    }
    UI_DYNAMIC {
        char counters[32];
        /* Version shares the return header; live counts use the blank band. */
        u8g2_SetDrawColor(&u8g2, 0u);
        snprintf(counters, sizeof(counters), "V%u", (unsigned)Data_list1.UI_main.version);
        ui_menu_42_text_right(382u, 34u, counters, 14u);
        snprintf(counters, sizeof(counters), "通道:%u", (unsigned)count_used_channels());
        ui_text_draw((uint16_t)(127u + (266u-ui_text_width(counters, 18u))/2u),
                     190u, counters, 18u);
        snprintf(counters, sizeof(counters), "绑定:%u台", (unsigned)g_binding_count);
        ui_text_draw((uint16_t)(127u + (266u-ui_text_width(counters, 18u))/2u),
                     226u, counters, 18u);
    }
}

static void UI_Menu_Display_42(void)
{
    uint8_t i;
    uint8_t bitmap_mode = u8g2.bitmap_transparency;
    uint8_t font_mode = u8g2.font_decode.is_transparent;
    uint8_t draw_color = u8g2.draw_color;
    if (UI_Select >= MAIN_LEN) UI_Select = 0;
    u8g2_SetBitmapMode(&u8g2, 1);
    u8g2_SetFontMode(&u8g2, 1);

    /* Guard subpage routes */
    if (Data_list1.menu_rank == 3)
    {
        uint8_t re_top = Data_list1.UI_main.re_flag;
        if (UI_Select == 2u && (re_top == 1u || re_top == 2u))
        {
            binding_Control_42();
            goto restore_style_42;
        }
        if (UI_Select == 3u && (re_top == 3u || re_top == 4u))
        {
            install_Control_42();
            goto restore_style_42;
        }
        if (UI_Select == 6u && (re_top == 5u || re_top == 6u))
        {
            summary_Control_42();
            goto restore_style_42;
        }
    }

    /* Left menu list */
    UI_STATIC {
        u8g2_SetDrawColor(&u8g2, 1);
        u8g2_DrawFrame(&u8g2, 2u, 2u, 396u, 296u);
        u8g2_DrawRFrame(&u8g2, 6u, 6u, 114u, 288u, 4u);
        u8g2_DrawFrame(&u8g2, 8u, 8u, 110u, 284u);
    }
    UI_DYNAMIC {
        for (i = 0u; i < MAIN_LEN; i++)
        {
            uint16_t top = (uint16_t)(10u + 35u * i);
            uint8_t selected = (uint8_t)(Data_list1.menu_rank == 2u && UI_Select == i);
            u8g2_SetDrawColor(&u8g2, 1);
            if (selected) ui_fast_DrawBox(&u8g2, 10u, top, 106u, 33u);
            else if (UI_Select == i) u8g2_DrawVLine(&u8g2, 10u, top, 33u);
            u8g2_SetDrawColor(&u8g2, selected ? 0u : 1u);
            ui_fast_DrawXBMP(&u8g2, 14u, (uint16_t)(top + 10u), 13u, 13u, ui_menu_nav_icons[i]);
            ui_text_draw(34u, (uint16_t)(top + 24u), Main_List[i].name, 16u);
        }
    }

    ui_menu_42_header(UI_Select);
    u8g2_SetDrawColor(&u8g2, 0u);

    switch (UI_Select)
    {
        case 0: addr_Control_42(); break;
        case 1: networking_Control_42(); break;
        case 2: binding_Control_42(); break;
        case 3: install_Control_42(); break;
        case 4: uploading_Control_42(); break;
        case 5: Other_Settings_Control_42(); break;
        case 6: summary_Control_42(); break;
        case 7: return_main_Control_42(); break;
        default: break;
    }

restore_style_42:
    if (ui_render_layer != UI_LAYER_STATIC &&
        (Data_list1.menu_rank == 2u || Data_list1.menu_rank == 3u))
    {
        uint8_t re = Data_list1.UI_main.re_flag;
        uint8_t full_screen = (uint8_t)((UI_Select == 1u && Data_list1.menu_rank == 3u &&
                                         Data_list1.Menu_rank2.net_flag != NET_FLAG_IDLE) ||
            (Data_list1.menu_rank == 3u &&
            ((UI_Select == 2u && (re == 1u || re == 2u)) ||
             (UI_Select == 3u && (re == 3u || re == 4u)) ||
             (UI_Select == 6u && (re == 5u || re == 6u)))));
        u8g2_SetDrawColor(&u8g2, 2u);
        if (full_screen) ui_fast_DrawBox(&u8g2, 2u, 2u, 396u, 296u);
        else ui_fast_DrawBox(&u8g2, 124u, 4u, 272u, 292u);
    }
    u8g2_SetBitmapMode(&u8g2, bitmap_mode);
    u8g2_SetFontMode(&u8g2, font_mode);
    u8g2_SetDrawColor(&u8g2, draw_color);
}

#endif /* UI_MENU_42_H */
