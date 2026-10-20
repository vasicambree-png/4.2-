/*
 * 4.2-inch ST7306 display UI layout (logical 400 x 600, physical 400 x 300).
 * This file is included by yuying_TFT.c AFTER the shared text/data helpers.
 * 2.9-inch drawing and the STM32 UART protocol remain completely unchanged.
 *
 * IMPORTANT: ST7306 encodes two U8g2 virtual vertical pixels per physical
 * vertical pixel with U8G2_R3. All y coordinates are therefore in 0..599.
 * Text glyphs are stretched vertically by ui_text_draw() in this profile.
 */
#if SCREEN_PANEL_ST7306_42

#define UI42_W           400u
#define UI42_H           600u
#define UI42_HEADER_Y     92u
#define UI42_LIST_Y      120u
#define UI42_FOOTER_Y    548u
#define UI42_RIGHT_X     141u
#define UI42_RIGHT_W     250u

static void ui42_text(uint16_t x, uint16_t y, const char *s, uint8_t size)
{
    ui_text_draw(x, y, s, size);
}

static void ui42_text_right(uint16_t right, uint16_t y, const char *s, uint8_t size)
{
    uint16_t width = ui_text_width(s, size);
    ui42_text((uint16_t)(width >= right ? 0u : right - width), y, s, size);
}

static void ui42_button(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                        const char *label, uint8_t selected, uint8_t center,
                        uint8_t size)
{
    uint16_t label_w = ui_text_width(label, size);
    uint16_t tx = (uint16_t)(x + 8u);
    uint16_t baseline = (uint16_t)(y + (h - (uint16_t)size * 2u) / 2u
                                  + (uint16_t)size * 2u - 2u);
    if (center && label_w + 8u < w) tx = (uint16_t)(x + (w - label_w) / 2u);
    u8g2_SetDrawColor(&u8g2, 1u);
    if (selected) u8g2_DrawBox(&u8g2, x, y, w, h);
    else u8g2_DrawFrame(&u8g2, x, y, w, h);
    u8g2_SetDrawColor(&u8g2, selected ? 0u : 1u);
    ui42_text(tx, baseline, label, size);
    u8g2_SetDrawColor(&u8g2, 1u);
}

static uint8_t ui42_selected(uint8_t index)
{
    return (uint8_t)(Data_list1.menu_rank == 3u &&
                     Data_list1.rank3_addr == index);
}

static void ui42_shell(const char *title, uint8_t full)
{
    u8g2_SetDrawColor(&u8g2, 1u);
    u8g2_DrawFrame(&u8g2, 2u, 2u, 396u, 596u);
    u8g2_DrawFrame(&u8g2, 6u, 6u, 388u, 588u);
    u8g2_DrawHLine(&u8g2, 8u, 88u, 384u);
    ui42_text(16u, 60u, title, 18u);
    if (full) {
        ui42_text_right(382u, 60u, "返回", 14u);
        u8g2_DrawHLine(&u8g2, 8u, UI42_FOOTER_Y, 384u);
    }
}

static void ui42_bottom_pair(const char *left, uint8_t left_sel,
                             const char *right, uint8_t right_sel)
{
    ui42_button(12u, 554u, 183u, 38u, left, left_sel, 1u, 14u);
    ui42_button(205u, 554u, 183u, 38u, right, right_sel, 1u, 14u);
}

static void ui42_menu_row(uint8_t id, const char *label)
{
    uint16_t y = (uint16_t)(122u + (uint16_t)id * 66u);
    ui42_button(UI42_RIGHT_X, y, UI42_RIGHT_W, 54u, label,
                ui42_selected(id), 0u, 16u);
}

static void ui42_menu_buttons(const char *left, uint8_t li,
                              const char *right, uint8_t ri)
{
    ui42_button(UI42_RIGHT_X, 511u, 120u, 62u, left,
                ui42_selected(li), 1u, 14u);
    ui42_button(271u, 511u, 120u, 62u, right,
                ui42_selected(ri), 1u, 14u);
}

static void ui42_menu_field(uint16_t y, const char *label, const char *value,
                            uint8_t label_id, uint8_t value_id)
{
    ui42_button(UI42_RIGHT_X, y, 130u, 60u, label,
                ui42_selected(label_id), 0u, 14u);
    ui42_button(278u, y, 113u, 60u, value,
                ui42_selected(value_id), 1u, 16u);
}

static void ui42_main(const data_LIST *p)
{
    char buf[48];
    uint8_t i, page, first;
    uint16_t y, x;
    u8g2_SetDrawColor(&u8g2, 1u);
    ui_main_cut_frame(2u, 2u, 396u, 596u, 11u);
    ui_main_cut_frame(6u, 6u, 388u, 588u, 8u);
    /* Retain the original brand bitmap; live values never enter assets. */
    ui_fast_DrawXBMP(&u8g2, 13u, 15u, UI_HOME_BRAND_WIDTH,
                     UI_HOME_BRAND_HEIGHT, ui_home_brand);
    ui42_text(125u, 47u, "正在通讯中", 18u);
    page = (p->UI_main.chu_num1 == 2u) ? 2u : 1u;

    u8g2_DrawHLine(&u8g2, 10u, 75u, 380u);
    if (page == 1u)
    {
        sprintf(buf, "分站号:%u", (unsigned int)p->UI_main.sub_num);
        ui42_text(14u, 111u, buf, 14u);
        sprintf(buf, "本机号:%u", (unsigned int)p->UI_main.host_num);
        ui42_text(158u, 111u, buf, 14u);
        sprintf(buf, "%u.%02uV", (unsigned int)(p->UI_main.vbat / 100u),
                (unsigned int)(p->UI_main.vbat % 100u));
        ui42_text_right(384u, 111u, buf, 14u);
        if (p->UI_main.send_host_num == 122u) strcpy(buf, "分站");
        else if (p->UI_main.send_host_num == 121u) strcpy(buf, "无");
        else if (p->UI_main.send_host_num == 0u) strcpy(buf, "中继");
        else sprintf(buf, "%u", (unsigned int)p->UI_main.send_host_num);
        ui42_text(14u, 153u, "正在通讯中", 14u);
        ui42_text(199u, 153u, buf, 14u);
        sprintf(buf, "信号:%u", (unsigned int)p->UI_main.Lora_rssi);
        ui42_text_right(384u, 153u, buf, 14u);
    }
    else
    {
        sprintf(buf, "已绑定:%u", (unsigned int)g_binding_count);
        ui42_text(14u, 111u, buf, 14u);
        sprintf(buf, "已用通道:%d", count_used_channels());
        ui42_text_right(385u, 111u, buf, 14u);
        ui42_text(14u, 153u,
                  p->UI_main.state == 1u ? "状态:开机" : "状态:关机", 14u);
    }

    /* 2 columns of 5 rows. Each 70 virtual rows = 35 physical pixels. */
    for (i = 0u; i < 2u; i++)
    {
        uint16_t cx = i ? 201u : 9u;
        ui_main_cut_frame(cx, 174u, 190u, 354u, 7u);
        for (uint8_t row = 1u; row < 5u; row++)
            u8g2_DrawHLine(&u8g2, cx,
                           (uint16_t)(174u + 70u * row), 190u);
    }

    first = (uint8_t)(page == 1u ? 0u : 10u);
    for (i = 0u; i < 10u; i++)
    {
        uint8_t ch = (uint8_t)(first + i);
        Sensor_Tpye type = ui_home_channel_type(ch);
        device_t *d = &CH_com_buf[ch];
        const char *unit = "";
        uint16_t unit_w, value_w, edge;
        x = (uint16_t)(15u + (i / 5u ? 192u : 0u));
        y = (uint16_t)(220u + 70u * (i % 5u));
        sprintf(buf, "%u", (unsigned int)(ch + 1u));
        ui42_text(x, y, buf, 14u);
        ui42_text((uint16_t)(x + 28u), y,
                  type > TPYE_NONE && type < TPYE_END ? CH_TYPE_NAME[type] : "--", 16u);
        if (type > TPYE_NONE && type < TPYE_END)
            unit = CH_TYPE_UNIT[type];
        if (type > TPYE_NONE && type < TPYE_END && d->valid && d->data_re_flag)
            ch_disp_format(buf, sizeof(buf), type, d->CH_data);
        else strcpy(buf, "--");
        edge = (uint16_t)(x + 176u);
        unit_w = ui_text_width(unit, 14u);
        if (unit_w)
        {
            ui42_text((uint16_t)(edge - unit_w), y, unit, 14u);
            edge = (uint16_t)(edge - unit_w - 3u);
        }
        value_w = ui_text_width(buf, 16u);
        if (edge >= value_w + x + 54u)
            ui42_text((uint16_t)(edge - value_w), y, buf, 16u);
        else
            ui42_text((uint16_t)(x + 85u), y, buf, 14u);
    }
    u8g2_DrawHLine(&u8g2, 10u, 545u, 380u);
    sprintf(buf, "%u/2", (unsigned int)page);
    ui42_text(18u, 580u, "通道", 14u);
    ui42_text_right(382u, 580u, buf, 14u);
}

static void ui42_nav(void)
{
    uint8_t i;
    u8g2_DrawVLine(&u8g2, 132u, UI42_HEADER_Y, 490u);
    for (i = 0u; i < MAIN_LEN; i++)
    {
        uint16_t y = (uint16_t)(104u + 60u * i);
        ui42_button(10u, y, 116u, 52u, Main_List[i].name,
                    (uint8_t)(Data_list1.menu_rank == 2u && UI_Select == i),
                    1u, 14u);
        if (Data_list1.menu_rank == 3u && UI_Select == i)
            u8g2_DrawVLine(&u8g2, 8u, y, 52u);
    }
}

static void ui42_device_list(uint8_t action_mode, uint8_t rank3)
{
    char buf[48];
    uint8_t n = g_binding_count;
    uint8_t columns, rows, i;
    uint16_t step, baseline, x, y, name_w;
    ui42_shell("设备绑定", 1u);
    sprintf(buf, "已绑定设备:%u", (unsigned int)g_binding_count);
    ui42_text_right(375u, 113u, buf, 14u);
    if (n > MAX_BINDING_NUM) n = MAX_BINDING_NUM;
    columns = n > 20u ? 4u : 2u;
    rows = columns == 4u ? 8u : 10u;
    step = (uint16_t)(380u / columns);
    name_w = (uint16_t)(step - 31u);
    for (i = 0u; i < n; i++)
    {
        x = (uint16_t)(13u + (uint16_t)(i / rows) * step);
        y = (uint16_t)(159u + (uint16_t)(i % rows) * (columns == 4u ? 46u : 39u));
        sprintf(buf, "%u:", (unsigned int)(i + 1u));
        ui42_text(x, y, buf, 11u);
        ui_draw_name_size((uint16_t)(x + 27u), y,
                          g_binding_list[i].name, name_w, 11u);
    }
    if (n == 0u) ui42_text(20u, 186u, "--", 18u);
    ui42_bottom_pair(action_mode == 1u ? "解绑" : "保存目前设备",
                     (uint8_t)(rank3 == 0u), "返回",
                     (uint8_t)(rank3 != 0u));
}

static void ui42_channel_list(uint8_t mode)
{
    uint8_t ch;
    char buf[36];
    ui42_shell(Menu_List[UI_Select].name, 1u);
    for (ch = 0u; ch < MAX_CH_NUM; ch++)
    {
        uint16_t x = ch < 10u ? 14u : 208u;
        uint16_t y = (uint16_t)(140u + (ch % 10u) * 40u);
        device_t *d = &CH_com_buf[ch];
        sprintf(buf, "%u", (unsigned int)(ch + 1u));
        ui42_text(x, y, buf, 14u);
        if (mode == 5u)
        {
            if (d->valid && d->name[0] != '\0')
                ui_draw_name_size((uint16_t)(x + 27u), y, d->name, 153u, 14u);
            else ui42_text((uint16_t)(x + 50u), y, "--", 14u);
        }
        else
        {
            if (!(d->valid && d->data_re_flag)) strcpy(buf, "--");
            else if (mode == 3u) sprintf(buf, "%d dBm", (int)(int8_t)d->rssi);
            else sprintf(buf, "%u.%u V", (unsigned int)(d->voltage / 10u),
                         (unsigned int)(d->voltage % 10u));
            ui42_text((uint16_t)(x + 46u), y, buf, 14u);
        }
        if (ch % 10u != 9u)
            u8g2_DrawHLine(&u8g2, (uint16_t)(x - 2u),
                           (uint16_t)(y + 9u), 183u);
    }
}

static void ui42_network_result(void)
{
    char buf[36];
    uint8_t i;
    uint16_t flag = Data_list1.Menu_rank2.net_flag;
    uint8_t running = (uint8_t)(flag != NET_FLAG_IDLE && flag != NET_FLAG_DONE);
    ui42_shell("组网测试", 1u);
    sprintf(buf, "本机地址:%u", (unsigned int)Data_list1.Menu_rank2.now_host_addr);
    ui42_text_right(382u, 113u, buf, 14u);
    ui42_text(20u, 166u, "测试地址", 16u);
    ui42_text(154u, 166u, "测试次数", 16u);
    ui42_text(293u, 166u, "成功比率", 16u);
    u8g2_DrawHLine(&u8g2, 10u, 185u, 380u);
    for (i = 0u; i < 3u; i++)
    {
        uint16_t y = (uint16_t)(215u + 98u * i);
        uint16_t cnt = Data_list1.Menu_rank2.cnt[i];
        uint16_t succ = Data_list1.Menu_rank2.succ[i];
        uint16_t pct = cnt ? (uint16_t)(((uint32_t)succ * 100u + cnt / 2u) / cnt) : 0u;
        uint8_t current = (uint8_t)(running && Data_list1.Menu_rank2.cur_index == i);
        if (current) u8g2_DrawFrame(&u8g2, 12u, (uint16_t)(y - 38u), 375u, 75u);
        if (Data_list1.Menu_rank2.addr[i] == 0u) strcpy(buf, "中继");
        else if (Data_list1.Menu_rank2.addr[i] == 121u) strcpy(buf, "无");
        else if (Data_list1.Menu_rank2.addr[i] == 122u) strcpy(buf, "分站");
        else sprintf(buf, "%u号", (unsigned int)Data_list1.Menu_rank2.addr[i]);
        ui42_text(25u, y, buf, 18u);
        sprintf(buf, "%u/10", (unsigned int)cnt);
        ui42_text(154u, y, buf, 18u);
        sprintf(buf, "%u%%", (unsigned int)pct);
        ui42_text(293u, y, buf, 18u);
    }
    ui42_bottom_pair(running ? "正在组网" :
                     flag == NET_FLAG_DONE ? "重测组网" : "开始组网",
                     (uint8_t)(!running && ui42_selected(0u)),
                     "返回", (uint8_t)(!running && ui42_selected(1u)));
}

static void ui42_detail(void)
{
    uint8_t re = Data_list1.UI_main.re_flag;
    uint8_t menu = UI_Select;
    uint8_t i;
    char a[40], b[40], c[40];

    switch (menu)
    {
    case 0u: /* address and substation configuration */
        sprintf(a, "%u", (unsigned int)Data_list1.Menu_rank1.set_sub_num);
        sprintf(b, "%u", (unsigned int)Data_list1.Menu_rank1.set_host_num);
        ui42_menu_field(157u, "分站号:", a, 0u, 1u);
        ui42_menu_field(245u, "本机地址:", b, 2u, 3u);
        ui42_menu_buttons("保存并返回", 4u, "不保存返回", 5u);
        break;
    case 1u:
        ui42_text(163u, 195u, "组网测试", 18u);
        ui42_menu_buttons("开始组网", 0u, "返回", 1u);
        break;
    case 2u:
        sprintf(a, "已绑定设备:%u台", (unsigned int)g_binding_count);
        ui42_menu_row(0u, a);
        ui42_menu_row(1u, "一键解绑");
        ui42_menu_row(2u, "绑定设备");
        ui42_menu_buttons("返回", 3u, "设备绑定", 2u);
        break;
    case 3u:
        ui42_menu_row(0u, "设备信号");
        ui42_menu_row(1u, "设备电压");
        ui42_menu_buttons("返回", 2u, "安装调试", 0u);
        break;
    case 4u:
        ui42_text(172u, 129u, "已存地址", 14u);
        ui42_text(293u, 129u, "更改为", 14u);
        for (i = 0u; i < 3u; i++)
        {
            uint16_t old = Data_list1.Menu_rank5.old_send_addr[i];
            uint16_t next = Data_list1.Menu_rank5.new_send_addr[i];
            uint16_t y = (uint16_t)(157u + 98u * i);
            sprintf(a, "上传%u", (unsigned int)(i + 1u));
            ui42_text(145u, (uint16_t)(y + 34u), a, 14u);
            if (old == 0u) strcpy(b, "中继");
            else if (old == 121u) strcpy(b, "无");
            else if (old == 122u) strcpy(b, "分站");
            else sprintf(b, "%u", (unsigned int)old);
            if (next == 0u) strcpy(c, "中继");
            else if (next == 121u) strcpy(c, "无");
            else if (next == 122u) strcpy(c, "分站");
            else sprintf(c, "%u", (unsigned int)next);
            ui42_text(199u, (uint16_t)(y + 34u), b, 14u);
            ui42_button(280u, y, 110u, 64u, c, ui42_selected(i), 1u, 14u);
        }
        ui42_menu_buttons("保存并返回", 3u, "不保存返回", 4u);
        break;
    case 5u:
        sprintf(a, "%u db", (unsigned int)Data_list1.Menu_rank6.power);
        ui42_menu_field(119u, "通信功率:", a, 0u, 1u);
        sprintf(a, "%u 秒", (unsigned int)Data_list1.Menu_rank6.time_light);
        ui42_menu_field(215u, "亮屏时间:", a, 2u, 3u);
        ui42_menu_field(311u, "通信状态:",
                        Data_list1.Menu_rank6.state == 1 ? "开机" : "不开机", 4u, 5u);
        ui42_menu_buttons("保存并返回", 6u, "不保存返回", 7u);
        break;
    case 6u:
        if (re == 6u)
        {
            ui42_text(148u, 225u, "确认恢复出厂设置?", 16u);
            ui42_menu_buttons("确认", 0u, "返回", 1u);
            break;
        }
        sprintf(a, "查看已绑定的设备:%u台", (unsigned int)g_binding_count);
        ui42_menu_row(0u, a);
        sprintf(a, "蓝牙名称错误警报:%u", (unsigned int)g_name_err_count);
        ui42_menu_row(1u, a);
        sprintf(a, "蓝牙电压异常警报:%u", (unsigned int)g_volt_err_count);
        ui42_menu_row(2u, a);
        ui42_menu_row(3u, "恢复出厂");
        ui42_menu_buttons("返回", 4u, "信息汇总", 0u);
        break;
    case 7u:
        ui42_text(156u, 246u, "返回主页", 18u);
        sprintf(a, "V1_%u_%u", (unsigned int)(Data_list1.UI_main.version / 10u),
                (unsigned int)(Data_list1.UI_main.version % 10u));
        ui42_text(188u, 316u, a, 18u);
        break;
    default: break;
    }
}

static void ui42_draw_scene(data_LIST *list)
{
    uint8_t re = list->UI_main.re_flag;
    uint8_t menu = list->rank2_addr;
    if (menu >= MAIN_LEN) menu = 0u;
    /* The controller alone decides menu and item selection. */
    UI_Select = menu;
    u8g2_SetDrawColor(&u8g2, 1u);
    u8g2_SetBitmapMode(&u8g2, 1u);

    if (g_ui_msg != UI_MSG_NONE)
    {
        UI_Message_Display();
        return;
    }
    if (list->menu_rank == 1u)
    {
        ui42_main(list);
        return;
    }
    if (list->menu_rank == 4u)
    {
        ui42_shell("正在重启保存数据", 1u);
        return;
    }
    if (list->menu_rank != 2u && list->menu_rank != 3u) return;

    if (list->menu_rank == 3u)
    {
        if (menu == 1u && list->Menu_rank2.net_flag != NET_FLAG_IDLE)
        {
            ui42_network_result();
            return;
        }
        if (menu == 2u && (re == 1u || re == 2u))
        {
            ui42_device_list(re, list->rank3_addr);
            return;
        }
        if (menu == 3u && (re == 3u || re == 4u))
        {
            ui42_channel_list(re);
            return;
        }
        if (menu == 6u && re == 5u)
        {
            ui42_channel_list(5u);
            return;
        }
    }

    ui42_shell(Menu_List[menu].name, 0u);
    ui42_nav();
    ui42_detail();
}

#endif /* SCREEN_PANEL_ST7306_42 */
