static unsigned checks, failures;
#define CHECK(c) do { checks++; if (!(c)) { failures++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static uint16_t make_page(uint8_t *out, uint8_t rank, uint8_t menu, uint8_t focus,
                          const uint16_t *params, uint8_t count)
{
    uint16_t len = (uint16_t)(6u + 2u * count), i;
    out[0] = 0xaa; out[1] = 0xee; out[2] = (uint8_t)(len >> 8);
    out[3] = (uint8_t)len; out[4] = 1; out[5] = 1;
    out[6] = rank; out[7] = menu; out[8] = focus; out[9] = count;
    for (i = 0; i < count; i++) {
        out[10 + i * 2] = (uint8_t)(params[i] >> 8);
        out[11 + i * 2] = (uint8_t)params[i];
    }
    out[10 + count * 2] = calculate_checksum(out + 4, len);
    out[11 + count * 2] = 0x0a;
    return (uint16_t)(len + 6);
}

static void enqueue(const uint8_t *frame, uint16_t len)
{
    if (queue_head == queue_tail) queue_head = queue_tail = 0;
    CHECK(queue_tail < 8);
    memcpy(queued[queue_tail], frame, len);
    queued_len[queue_tail++] = len;
}

static void pump(unsigned ticks)
{
    unsigned i;
    for (i = 0; i < ticks; i++) usart_ProcessEvent(0, START_IO_EVT);
    usart_ProcessEvent(0, START_DATA_EVT);
}

static void reset(void)
{
    memset(&Data_list1, 0, sizeof(Data_list1));
    memset(&screen_power, 0, sizeof(screen_power));
    s_binding_save_return_pending = 0;
    memset(&last_drawn, 0, sizeof(last_drawn));
    mock_clock = 0; draws = off_calls = on_calls = drains = round_ticks = report_calls = 0;
    cmd03_requests = cmd03_ticks = cmd03_answers = response_calls = 0;
    init_calls = saves = clears = ble_ticks = name_ticks = 0;
    mock_save_status = last_message = last_response_cmd = last_response_status = 0;
    host_real_ui_show_msg(UI_MSG_NONE);
    last_response_len = 0;
    mock_round_complete = mock_cmd03_ready = 0;
    g_scan_mode = g_binding_count = g_store_dirty = 0;
    mock_clear_had = 1; ram_clear_calls = 0;
    Rx_sleep_flag = 1; dis_flag_cnt = 0; g_frame_last_sec = FRAME_AGE_NEVER;
    ui_refresh_tick = ui_tick_10ms = ui_last_redraw_10ms = 0;
    g_ui_reinit_req = g_store_clear_req = 0;
    fifo_len_flag = 0; fifo_time_Cnt = 0; queue_head = queue_tail = 0;
}

int main(void)
{
    uint8_t frame[128], bad[128];
    uint16_t settings[] = { 2, 10, 1, 1, 0 };
    uint16_t home[] = { 101, 3200, 3, 4, 5, 6, 77, 1, 1, 0, 1234 };
    uint16_t len;
    unsigned old_draws, old_drains, i;
    data_LIST saved;

    reset();
    len = make_page(frame, 2, 5, 1, settings, 5);
    enqueue(frame, len); app_uart_process(); pump(5);
    CHECK(Data_list1.Menu_rank6.time_light == 10);
    CHECK(on_calls == 1 && Rx_sleep_flag == 0 && draws == 1);

    mock_clock = 15999;
    settings[0] = 3; /* live setting value unrelated to timeout/navigation */
    len = make_page(frame, 2, 5, 1, settings, 5);
    enqueue(frame, len); app_uart_process(); pump(5);
    CHECK(off_calls == 0 && Rx_sleep_flag == 0);
    mock_clock = 16000; pump(5);
    CHECK(off_calls == 1 && Rx_sleep_flag == 1);
    old_draws = draws; old_drains = drains;
    for (i = 0; i < 3; i++) {
        settings[0] = (uint16_t)(4 + i);
        len = make_page(frame, 2, 5, 1, settings, 5);
        enqueue(frame, len); app_uart_process(); pump(20);
    }
    CHECK(Data_list1.Menu_rank6.power == 6);
    CHECK(draws == old_draws && on_calls == 1 && off_calls == 1);
    CHECK(drains == old_drains + 60);
    puts("PASS: 10-second boundary, power-save transition, settings cache and BLE drain while off");

    /* Main page values update while off; no screen wake or timeout refresh. */
    settings[0] = 2;
    mock_clock = 17000;
    len = make_page(frame, 1, 0, 0, home, 11);
    enqueue(frame, len); app_uart_process(); pump(5);
    CHECK(Rx_sleep_flag == 0 && on_calls == 2);
    mock_clock = 33000; pump(5);
    CHECK(Rx_sleep_flag == 1 && off_calls == 2);
    old_draws = draws;
    for (i = 0; i < 4; i++) {
        home[1] = (uint16_t)(3300 + i); home[10] = (uint16_t)(2000 + i);
        len = make_page(frame, 1, 0, 0, home, 11);
        enqueue(frame, len);
    }
    app_uart_process(); pump(5);
    CHECK(queue_head == queue_tail);
    CHECK(Data_list1.UI_main.vbat == 3303 && Data_list1.UI_main.data[0] == 2003);
    CHECK(Rx_sleep_flag == 1 && draws == old_draws && on_calls == 2);
    puts("PASS: actual app_uart_process drains four frames and stores latest home data while off");

    { uint8_t request[] = { 0xaa, 0xee, 0, 1, 3, 3, 0x0a };
      enqueue(request, sizeof(request)); app_uart_process(); }
    CHECK(cmd03_requests == 1);
    g_binding_count = 1; mock_round_complete = mock_cmd03_ready = 1; g_store_dirty = 1;
    usart_ProcessEvent(0, START_TIMER_EVT);
    CHECK(cmd03_ticks == 1 && cmd03_answers == 1 && report_calls == 1);
    CHECK(round_ticks == 1 && ble_ticks == 1 && name_ticks == 1 && saves == 1);
    CHECK(Rx_sleep_flag == 1);
    puts("PASS: 0x03 request/answer, 0x06 report, Flash and BLE/name heartbeat continue while off");

    home[10] = 9999;
    len = make_page(frame, 1, 0, 1, home, 11);
    enqueue(frame, len); app_uart_process(); pump(5);
    CHECK(Rx_sleep_flag == 0 && on_calls == 3 && draws == old_draws + 1);
    CHECK(last_drawn.UI_main.vbat == 3303 && last_drawn.UI_main.data[0] == 9999);
    puts("PASS: menu focus change wakes once and redraws latest cached data");

    saved = Data_list1;
    mock_clock = 49000;
    memcpy(bad, frame, len); bad[len - 2] ^= 0xff;
    CHECK(parse_received_frame(bad, len, &Data_list1) != 0);
    CHECK(memcmp(&saved, &Data_list1, sizeof(saved)) == 0);
    len = make_page(bad, 1, 0, 2, home, 9); /* checksum-valid but incomplete home */
    CHECK(parse_received_frame(bad, len, &Data_list1) != 0);
    CHECK(memcmp(&saved, &Data_list1, sizeof(saved)) == 0);
    CHECK(parse_received_frame(NULL, 0, &Data_list1) != 0);
    pump(5);
    CHECK(Rx_sleep_flag == 1 && off_calls == 3 && on_calls == 3);
    puts("PASS: malformed/short frames do not mutate cache, wake display or restart timeout");

    /* A full clock cycle cannot silently reopen the display after timeout. */
    mock_clock = 33000;
    len = make_page(frame, 1, 0, 1, home, 11);
    enqueue(frame, len); app_uart_process(); pump(5);
    CHECK(Rx_sleep_flag == 1 && on_calls == 3 && off_calls == 3);
    puts("PASS: expired display stays off after clock wraps with ordinary refresh");

    mock_clock = 50000; settings[1] = 0;
    len = make_page(frame, 2, 5, 1, settings, 5);
    enqueue(frame, len); app_uart_process(); pump(5);
    mock_clock = 1000000; pump(5);
    CHECK(Rx_sleep_flag == 0);
    puts("PASS: time_light=0 keeps display on");

    reset(); settings[1] = 10;
    len = make_page(frame, 2, 5, 1, settings, 5);
    enqueue(frame, len); app_uart_process(); pump(5);
    mock_clock = 16000; pump(5);
    saved = Data_list1;
    for (i = 3; i <= 4; i++) {
        uint32_t activity = screen_power.last_activity;
        mock_clock += 100;
        len = make_page(frame, 3, 5, 2, settings, (uint8_t)i);
        CHECK(parse_received_frame(frame, len, &Data_list1) != 0);
        CHECK(memcmp(&saved, &Data_list1, sizeof(saved)) == 0);
        CHECK(screen_power.last_activity == activity);
        pump(5);
        CHECK(Rx_sleep_flag == 1 && on_calls == 1 && off_calls == 1);
    }
    puts("PASS: checksum-valid settings missing one/both metadata fields cannot mutate or wake");

    mock_clock = 17000;
    len = make_page(frame, 5, 0, 0, NULL, 0);
    enqueue(frame, len); app_uart_process();
    CHECK(g_ui_reinit_req == 1 && init_calls == 0);
    pump(5);
    CHECK(Rx_sleep_flag == 0 && on_calls == 2);
    usart_ProcessEvent(0, START_TIMER_EVT); pump(5);
    CHECK(init_calls == 1 && g_ui_reinit_req == 0 && Rx_sleep_flag == 0);
    puts("PASS: explicit rank5 reinitialization wakes and runs in deferred timer event");

    saved = Data_list1;
    { uint32_t activity = screen_power.last_activity;
      uint16_t message[] = { UI_MSG_PARAM_SAVE_OK };
      mock_clock = 32999;
      len = make_page(frame, 6, 5, 2, message, 1);
      enqueue(frame, len); app_uart_process();
      CHECK(screen_power.last_activity == activity);
      CHECK(memcmp(&saved, &Data_list1, sizeof(saved)) == 0); }
    mock_clock = 33000; pump(5);
    CHECK(Rx_sleep_flag == 1 && off_calls == 2 && on_calls == 2);
    puts("PASS: rank6 message keeps cached page and does not extend screen timeout");

    mock_clock = 34000;
    len = make_page(frame, 5, 0, 0, NULL, 0);
    enqueue(frame, len); app_uart_process();
    CHECK(g_ui_reinit_req == 1);
    /* Hold the deferred initializer until the requested display time has expired. */
    mock_clock = 50000;
    usart_ProcessEvent(0, START_TIMER_EVT); pump(5);
    CHECK(init_calls == 1 && g_ui_reinit_req == 0);
    CHECK(Rx_sleep_flag == 1 && off_calls == 2 && on_calls == 2);
    puts("PASS: expired deferred rank5 request cannot reinitialize or reopen display");

    /* Return-home is menu 7, with only the two shared metadata words. */
    reset();
    {
        uint16_t metadata[] = { 2, 5 };
        screen_power_t policy;
        uint8_t selected;
        len = make_page(frame, 2, 7, 0, metadata, 2);
        CHECK(len == 16);
        enqueue(frame, len); app_uart_process(); pump(5);
        CHECK(Data_list1.menu_rank == 2 && Data_list1.rank2_addr == 7 && UI_Select == 7);
        CHECK(Data_list1.UI_main.chu_num2 == 2 && Data_list1.UI_main.re_flag == 5);
        CHECK(last_drawn.menu_rank == 2 && last_drawn.rank2_addr == 7 && draws == 1);
        CHECK(screen_power.seen && screen_power.menu == 7 && g_frame_last_sec == 0);

        metadata[0] = 1; metadata[1] = 4;
        len = make_page(frame, 3, 7, 1, metadata, 2);
        enqueue(frame, len); app_uart_process(); pump(5);
        CHECK(last_drawn.menu_rank == 3 && last_drawn.rank2_addr == 7 && last_drawn.rank3_addr == 1);
        CHECK(last_drawn.UI_main.chu_num2 == 1 && last_drawn.UI_main.re_flag == 4);
        CHECK(draws == 2 && UI_Select == 7);

        saved = Data_list1; policy = screen_power; selected = UI_Select;
        g_frame_last_sec = 9;
        mock_clock += 100;
        for (i = 0; i < 2; ++i) {
            len = make_page(bad, 3, 7, 0, metadata, (uint8_t)i);
            CHECK(parse_received_frame(bad, len, &Data_list1) != 0);
            CHECK(memcmp(&saved, &Data_list1, sizeof(saved)) == 0);
            CHECK(memcmp(&policy, &screen_power, sizeof(policy)) == 0);
            CHECK(UI_Select == selected && g_frame_last_sec == 9);
        }
        len = make_page(bad, 3, 8, 0, metadata, 2);
        CHECK(parse_received_frame(bad, len, &Data_list1) != 0);
        CHECK(memcmp(&saved, &Data_list1, sizeof(saved)) == 0);
        CHECK(memcmp(&policy, &screen_power, sizeof(policy)) == 0);
        CHECK(UI_Select == selected && g_frame_last_sec == 9);
        len = make_page(bad, 3, 7, 0, metadata, 2);
        CHECK(parse_received_frame(bad, (uint16_t)(len - 1), &Data_list1) != 0);
        bad[len - 2] ^= 0xff;
        CHECK(parse_received_frame(bad, len, &Data_list1) != 0);
        CHECK(memcmp(&saved, &Data_list1, sizeof(saved)) == 0);
        CHECK(memcmp(&policy, &screen_power, sizeof(policy)) == 0);
        CHECK(UI_Select == selected && g_frame_last_sec == 9);

        /* The controller completes confirmation with its normal home frame. */
        len = make_page(frame, 1, 0, 0, home, 11);
        enqueue(frame, len); app_uart_process(); pump(5);
        CHECK(last_drawn.menu_rank == 1 && Data_list1.menu_rank == 1 && draws == 3);
        len = make_page(frame, 2, 7, 0, metadata, 2);
        enqueue(frame, len); app_uart_process(); pump(5);
        CHECK(last_drawn.menu_rank == 2 && last_drawn.rank2_addr == 7 && draws == 4);
    }
    puts("PASS: return-home menu 7 accepts metadata, rejects incomplete/unknown frames and reenters after home");

    reset(); settings[1] = 10;
    len = make_page(frame, 2, 5, 1, settings, 5);
    enqueue(frame, len); app_uart_process(); pump(5);
    mock_clock = 16000; pump(5);
    CHECK(Rx_sleep_flag == 1 && off_calls == 1);
    len = make_page(bad, 3, 5, 1, settings, 4);
    CHECK(parse_received_frame(bad, len, &Data_list1) != 0);
    pump(5);
    CHECK(Rx_sleep_flag == 1 && screen_power.rank == 2);
    mock_clock = 17000;
    len = make_page(frame, 3, 5, 1, settings, 5);
    enqueue(frame, len); app_uart_process(); pump(5);
    CHECK(Rx_sleep_flag == 0 && on_calls == 2 && screen_power.rank == 3);
    mock_clock = 33000; pump(5);
    CHECK(Rx_sleep_flag == 0 && off_calls == 1);
    mock_clock = 1000000;
    enqueue(frame, len); app_uart_process(); pump(5);
    CHECK(Rx_sleep_flag == 0 && off_calls == 1 && on_calls == 2);
    CHECK(screen_power.last_activity == 17000 && screen_power.expired == 0);
    g_frame_last_sec = UI_HOLD_SEC + 1; old_draws = draws;
    mock_clock = 2000000; pump(5);
    CHECK(Rx_sleep_flag == 0 && off_calls == 1 && draws == old_draws);
    { uint16_t message[] = { UI_MSG_PARAM_SAVE_OK };
      len = make_page(frame, 6, 5, 1, message, 1);
      enqueue(frame, len); app_uart_process(); pump(5);
      CHECK(Rx_sleep_flag == 0 && screen_power.rank == 3); }
    len = make_page(frame, 5, 0, 0, NULL, 0);
    enqueue(frame, len); app_uart_process();
    mock_clock += 16000;
    usart_ProcessEvent(0, START_TIMER_EVT); pump(5);
    CHECK(init_calls == 1 && Rx_sleep_flag == 0 && screen_power.rank == 3);
    len = make_page(frame, 2, 5, 1, settings, 5);
    enqueue(frame, len); app_uart_process(); pump(5);
    CHECK(screen_power.last_activity == mock_clock && screen_power.rank == 2);
    mock_clock += 15999; pump(5);
    CHECK(Rx_sleep_flag == 0 && off_calls == 1);
    mock_clock += 1; pump(5);
    CHECK(Rx_sleep_flag == 1 && off_calls == 2 && on_calls == 2);
    puts("PASS: valid rank3 stays on through timeout/stale frames/messages/reinit; exit restarts timeout");
    /* Drive the real UART parser, retaining each Flash status and response. */
    for (i = 0; i < 3; i++) {
        uint16_t binding[16] = {0};
        uint8_t save_frame[] = {0xaa, 0xee, 0, 1, 5, 5, 0x0a};
        const uint8_t messages[] = {
            UI_MSG_BIND_SAVE_OK, UI_MSG_BIND_SAVE_EMPTY, UI_MSG_BIND_SAVE_FAIL
        };
        reset(); binding[14] = 1; binding[15] = 2;
        Data_list1.Menu_rank6.time_light = 10;
        len = make_page(frame, 3, 2, 7, binding, 16);
        enqueue(frame, len); app_uart_process();
        usart_ProcessEvent(0, START_TIMER_EVT);
        CHECK(g_scan_mode == SCAN_MODE_BIND && screen_power.rank == 3);
        mock_clock = 20000; mock_save_status = (uint8_t)i;
        enqueue(save_frame, sizeof(save_frame)); app_uart_process();
        CHECK(saves == 1 && response_calls == 1 && last_response_cmd == 5);
        CHECK(last_response_len == 1 && last_response_status == i);
        CHECK(last_message == messages[i]);
        CHECK(Data_list1.menu_rank == 2 && Data_list1.rank2_addr == 2 &&
              Data_list1.rank3_addr == 2 && Data_list1.UI_main.re_flag == 0);
        CHECK(Data_list1.UI_main.chu_num2 == 1 && g_scan_mode == SCAN_MODE_DATA);
        CHECK(screen_power.rank == 2 && screen_power.menu == 2 &&
              screen_power.focus == 2 && screen_power.subpage == 0 &&
              screen_power.last_activity == mock_clock);
        CHECK(dis_flag_cnt == 1);
        mock_clock += 15999;
        CHECK(screen_power_is_off(&screen_power, mock_clock) == 0);
        mock_clock++;
        CHECK(screen_power_is_off(&screen_power, mock_clock) == 1);
    }
    /* Invalid requests have no side effects; saves elsewhere preserve navigation. */
    for (i = 0; i < 5; i++) {
        uint8_t save_frame[] = {0xaa, 0xee, 0, 1, 5, 5, 0x0a, 0};
        uint16_t save_len = 7;
        screen_power_t policy;
        reset(); Data_list1.menu_rank = 3; Data_list1.rank2_addr = 2;
        Data_list1.rank3_addr = 7; Data_list1.UI_main.re_flag = 2;
        if (i == 0) save_frame[5] = 4;
        else if (i == 1) {
            save_frame[3] = 2; save_frame[5] = 0;
            save_frame[6] = 5; save_frame[7] = 0x0a; save_len = 8;
        }
        else if (i == 2) Data_list1.menu_rank = 2;
        else if (i == 3) Data_list1.rank2_addr = 1;
        else Data_list1.UI_main.re_flag = 1;
        saved = Data_list1; policy = screen_power; g_scan_mode = SCAN_MODE_BIND;
        enqueue(save_frame, save_len); app_uart_process();
        CHECK(memcmp(&saved, &Data_list1, sizeof(saved)) == 0);
        CHECK(memcmp(&policy, &screen_power, sizeof(policy)) == 0);
        CHECK(g_scan_mode == SCAN_MODE_BIND);
        CHECK(saves == (i >= 2 ? 1u : 0u) && response_calls == (i >= 2 ? 1u : 0u));
        CHECK(s_binding_save_return_pending == 0);
    }
    puts("PASS: binding save returns to parent with immediate data scan/power policy; statuses, invalid requests and unrelated pages preserved");
    for (i = 0; i < 2; i++) {
        uint8_t clear_frame[] = {0xaa, 0xee, 0, 1, 4, 4, 0x0a};
        reset(); Data_list1.menu_rank = 3; Data_list1.rank2_addr = 2;
        Data_list1.rank3_addr = 0; Data_list1.UI_main.re_flag = 1;
        Data_list1.UI_main.chu_num2 = 1; Data_list1.Menu_rank6.time_light = 2;
        g_binding_count = 3; mock_clear_had = (uint8_t)i; mock_clock = 20000;
        enqueue(clear_frame, sizeof(clear_frame)); app_uart_process();
        CHECK(ram_clear_calls == 1 && g_binding_count == 0);
        CHECK(response_calls == 1 && last_response_cmd == 4 &&
              last_response_len == 1 && last_response_status == (i ? 0 : 1));
        CHECK(g_store_clear_req == 1 && clears == 0);
        CHECK(Data_list1.menu_rank == 2 && Data_list1.rank2_addr == 2 &&
              Data_list1.rank3_addr == 1 && Data_list1.UI_main.re_flag == 0);
        CHECK(g_scan_mode == SCAN_MODE_DATA && dis_flag_cnt == 1);
        CHECK(screen_power.rank == 2 && screen_power.menu == 2 &&
              screen_power.focus == 1 && screen_power.subpage == 0 &&
              screen_power.last_activity == mock_clock);
        usart_ProcessEvent(0, START_TIMER_EVT);
        CHECK(clears == 1 && g_store_clear_req == 0 && last_message == UI_MSG_BIND_CLEAR_OK);
        CHECK(Data_list1.menu_rank == 2 && Data_list1.UI_main.re_flag == 0);
    }
    for (i = 0; i < 5; i++) {
        uint8_t clear_frame[] = {0xaa, 0xee, 0, 1, 4, 4, 0x0a, 0};
        uint16_t clear_len = 7;
        screen_power_t policy;
        reset(); Data_list1.menu_rank = 3; Data_list1.rank2_addr = 2;
        Data_list1.rank3_addr = 0; Data_list1.UI_main.re_flag = 1;
        if (i == 0) clear_frame[5] = 5;
        else if (i == 1) {
            clear_frame[3] = 2; clear_frame[5] = 0;
            clear_frame[6] = 4; clear_frame[7] = 0x0a; clear_len = 8;
        }
        else if (i == 2) Data_list1.menu_rank = 2;
        else if (i == 3) Data_list1.rank2_addr = 1;
        else Data_list1.UI_main.re_flag = 2;
        saved = Data_list1; policy = screen_power; g_scan_mode = SCAN_MODE_DATA;
        enqueue(clear_frame, clear_len); app_uart_process();
        CHECK(memcmp(&saved, &Data_list1, sizeof(saved)) == 0);
        CHECK(memcmp(&policy, &screen_power, sizeof(policy)) == 0);
        CHECK(ram_clear_calls == (i >= 2 ? 1u : 0u));
        CHECK(g_store_clear_req == (i >= 2 ? 1u : 0u));
        CHECK(response_calls == (i >= 2 ? 1u : 0u));
    }
    puts("PASS: unbind returns to parent; deferred flash clear, statuses and invalid/unrelated requests preserved");
    /* Save followed by the controller's stale detail frames, including
     * within one UART burst and after the real message hold expires. */
    for (i = 0; i < 3; i++) {
        uint16_t binding[16] = {0};
        uint8_t save_frame[] = {0xaa, 0xee, 0, 1, 5, 5, 0x0a};
        unsigned tick;
        reset(); mock_save_status = (uint8_t)i;
        binding[14] = 1; binding[15] = 2;
        len = make_page(frame, 3, 2, 0, binding, 16);
        enqueue(frame, len); app_uart_process();
        enqueue(save_frame, sizeof(save_frame)); enqueue(frame, len);
        app_uart_process();
        CHECK(saves == 1 && g_ui_msg != UI_MSG_NONE);
        CHECK(Data_list1.menu_rank == 2 && Data_list1.UI_main.re_flag == 0);
        for (tick = 0; tick < 4; tick++) {
            mock_clock += 1600;
            binding[0] = (uint16_t)(100u + tick);
            len = make_page(frame, 3, 2, 0, binding, 16);
            enqueue(frame, len); app_uart_process();
            usart_ProcessEvent(0, START_TIMER_EVT); pump(20);
            CHECK(Data_list1.menu_rank == 2 && Data_list1.rank3_addr == 2 &&
                  Data_list1.UI_main.re_flag == 0);
            CHECK(g_scan_mode == SCAN_MODE_DATA && screen_power.rank == 2);
            CHECK(Data_list1.Menu_rank3.biaoding_ad[0][0] == 100u + tick);
        }
        CHECK(g_ui_msg == UI_MSG_NONE && last_drawn.menu_rank == 2);
        CHECK(s_binding_save_return_pending == 1);
        /* Neither corrupt pages nor messages/reinitialization acknowledge a
         * controller page transition. Suppressed refresh does not renew power. */
        {
            uint32_t activity = screen_power.last_activity;
            uint16_t message[] = {UI_MSG_PARAM_SAVE_OK};
            len = make_page(frame, 2, 2, 2, binding, 16);
            frame[len - 2] ^= 1;
            enqueue(frame, len); app_uart_process();
            CHECK(s_binding_save_return_pending == 1);
            len = make_page(frame, 6, 2, 0, message, 1);
            enqueue(frame, len); app_uart_process();
            CHECK(s_binding_save_return_pending == 1);
            len = make_page(frame, 3, 2, 0, binding, 16);
            enqueue(frame, len); app_uart_process();
            CHECK(screen_power.last_activity == activity);
            len = make_page(frame, 5, 2, 0, NULL, 0);
            enqueue(frame, len); app_uart_process();
            CHECK(s_binding_save_return_pending == 1);
            len = make_page(frame, 3, 2, 0, binding, 16);
            enqueue(frame, len); app_uart_process();
            CHECK(Data_list1.menu_rank == 2 && Data_list1.UI_main.re_flag == 0);
        }
        /* Genuine parent, home or another detail page permits a fresh entry. */
        if (i == 0) len = make_page(frame, 2, 2, 2, binding, 16);
        else if (i == 1) len = make_page(frame, 1, 0, 0, home, 11);
        else len = make_page(frame, 3, 7, 0, binding, 2);
        enqueue(frame, len); app_uart_process();
        CHECK(s_binding_save_return_pending == 0);
        len = make_page(frame, 3, 2, 0, binding, 16);
        enqueue(frame, len); app_uart_process();
        CHECK(Data_list1.menu_rank == 3 && Data_list1.UI_main.re_flag == 2);
        usart_ProcessEvent(0, START_TIMER_EVT);
        CHECK(g_scan_mode == SCAN_MODE_BIND);
    }
    puts("PASS: save stays at parent through stale page bursts and real message expiry; data caching, power policy and acknowledged reentry preserved");
    printf("%u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
