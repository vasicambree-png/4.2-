#ifndef UI_RENDER_LAYERS_H
#define UI_RENDER_LAYERS_H

/* ALL retains the reference renderer. STATIC is used only by the offline
 * resource generator; DYNAMIC paints live fields over a restored background. */
enum { UI_LAYER_ALL, UI_LAYER_STATIC, UI_LAYER_DYNAMIC };
static uint8_t ui_render_layer = UI_LAYER_ALL;
#define UI_STATIC if (ui_render_layer != UI_LAYER_DYNAMIC)
#define UI_DYNAMIC if (ui_render_layer != UI_LAYER_STATIC)

#ifndef UI_STATIC_PAGES
#define UI_STATIC_PAGES (SCREEN_PANEL_ST7306_42 && SCREEN_ST7306_PAGE_BUFFER)
#endif

/* Keys describe artwork, never measurements, names, counts or focus. */
enum {
    UI_BG_HOME, UI_BG_MENU0, UI_BG_MENU1, UI_BG_MENU2, UI_BG_MENU3,
    UI_BG_MENU4, UI_BG_MENU5, UI_BG_MENU6, UI_BG_MENU7,
    UI_BG_BIND_REMOVE, UI_BG_BIND_SCAN, UI_BG_PARAM_INSTALL,
    UI_BG_PARAM_NAMES, UI_BG_FACTORY, UI_BG_NETWORK, UI_BG_RESTART,
    UI_BG_MESSAGE1, UI_BG_COUNT = UI_BG_MESSAGE1 + 9,
    UI_BG_INVALID = 255
};

#endif
