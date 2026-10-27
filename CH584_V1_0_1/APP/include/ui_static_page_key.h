#ifndef UI_STATIC_PAGE_KEY_H
#define UI_STATIC_PAGE_KEY_H

static uint8_t ui_static_page_key(void)
{
    uint8_t rank = Data_list1.menu_rank;
    uint8_t menu = Data_list1.rank2_addr;
    uint8_t re = Data_list1.UI_main.re_flag;
    if (g_ui_msg != UI_MSG_NONE)
        return g_ui_msg <= UI_MSG_SHUTTING_DOWN ?
            (uint8_t)(UI_BG_MESSAGE1 + g_ui_msg - 1u) : UI_BG_INVALID;
    if (rank == 1u) return UI_BG_HOME;
    if (rank == 4u) return UI_BG_RESTART;
    if (rank != 2u && rank != 3u) return UI_BG_INVALID;
    if (menu >= MAIN_LEN) menu = 0u;
    if (rank == 3u && menu == 1u && Data_list1.Menu_rank2.net_flag != NET_FLAG_IDLE)
        return UI_BG_NETWORK;
    if (rank == 3u && menu == 2u && re == 1u) return UI_BG_BIND_REMOVE;
    if (rank == 3u && menu == 2u && re == 2u) return UI_BG_BIND_SCAN;
    if (menu == 3u && (re == 3u || re == 4u)) return UI_BG_PARAM_INSTALL;
    if (menu == 6u && re == 5u) return UI_BG_PARAM_NAMES;
    if (menu == 6u && re == 6u) return UI_BG_FACTORY;
    return (uint8_t)(UI_BG_MENU0 + menu);
}

#endif
