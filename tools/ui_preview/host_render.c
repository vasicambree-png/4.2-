/* Offline framebuffer endpoint for the project's actual UI and u8g2 sources. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "yuying_TFT.h"
#include "observer.h"

u8g2_t u8g2;
device_t CH_com_buf[MAX_CH_NUM];
scan_binding g_binding_list[MAX_BINDING_NUM];
scan_name_entry_t g_scan_name_cache[SCAN_NAME_CACHE_NUM];
uint8_t g_binding_count, g_name_err_count, g_volt_err_count;
uint32_t g_name_chk_sec;
const u8g2_cb_t u8g2_cb_r3 = {0};
extern data_LIST Data_list1;
extern volatile uint8_t g_ui_msg;
extern u8g2_uint_t real_u8g2_DrawUTF8(u8g2_t *, u8g2_uint_t, u8g2_uint_t, const char *);
extern void real_u8g2_DrawXBM(u8g2_t *, u8g2_uint_t, u8g2_uint_t, u8g2_uint_t, u8g2_uint_t, const uint8_t *);
extern void real_u8g2_DrawXBMP(u8g2_t *, u8g2_uint_t, u8g2_uint_t, u8g2_uint_t, u8g2_uint_t, const uint8_t *);
extern void real_u8g2_DrawFrame(u8g2_t *, u8g2_uint_t, u8g2_uint_t, u8g2_uint_t, u8g2_uint_t);
extern void real_u8g2_DrawBox(u8g2_t *, u8g2_uint_t, u8g2_uint_t, u8g2_uint_t, u8g2_uint_t);
extern void real_u8g2_DrawRFrame(u8g2_t *, u8g2_uint_t, u8g2_uint_t, u8g2_uint_t, u8g2_uint_t, u8g2_uint_t);
extern void real_u8g2_DrawRBox(u8g2_t *, u8g2_uint_t, u8g2_uint_t, u8g2_uint_t, u8g2_uint_t, u8g2_uint_t);

static uint8_t pixels[SCREEN_HEIGHT][SCREEN_WIDTH];
static const char *case_name = "init";
static const char *draw_pass = "primary";
static int text_active, text_left, text_top, text_right, text_bottom;
static unsigned text_points;
static unsigned missing_total, boundary_total, mutation_total, redraw_total, cases_total;
static unsigned bitmap_state_total;
static unsigned region_violation_total, badge_violation_total;
static unsigned home_page_expect;   /* 0 = 不做主页分页契约检查，1/2 = 期望的主页页码 */
static unsigned home_p1_status_min_ink = 0xffffffffu, home_p1_stats_max_ink;
static unsigned home_p2_status_max_ink, home_p2_stats_min_ink = 0xffffffffu;
static unsigned badge_count, home_state_count, home_stats_count;
static char badge_text[32][4];
static FILE *draw_log, *case_log;
static int custom_glyph_active, custom_left, custom_top, custom_right, custom_bottom;
static unsigned custom_points;
static unsigned home_logo_count, home_logo_width, home_logo_height, home_new_layout;
static unsigned home_header_bottom, home_state_baseline, home_stats_baseline;
static unsigned home_relocated_layout, home_wifi_count;
static unsigned home_wifi_x, home_wifi_y;
static unsigned home_wifi_width, home_wifi_height;

/* 主页分页契约检查区域（像素坐标，含边界）。
 *   状态行带：第 1 页必须有墨迹，第 2 页必须全白；
 *   底部统计栏带：第 2 页必须有墨迹，第 1 页必须全白。
 * x 起点避开左侧品牌 Logo（x=9..80），终点避开双层外框与切角连接线。 */
#define HOME_STATUS_REGION_X0  83u
#define HOME_STATUS_REGION_Y0  29u
#define HOME_STATUS_REGION_X1 371u
#define HOME_STATUS_REGION_Y1  45u
#define HOME_STATS_REGION_X0   13u
#define HOME_STATS_REGION_Y0  147u
#define HOME_STATS_REGION_X1  370u
#define HOME_STATS_REGION_Y1  162u

static unsigned ink_in_region(unsigned x0, unsigned y0, unsigned x1, unsigned y1) {
    unsigned x, y, n = 0;
    for (y = y0; y <= y1; ++y)
        for (x = x0; x <= x1; ++x) n += pixels[y][x] ? 1u : 0u;
    return n;
}

uint8_t count_used_channels(void) {
    uint8_t i, n = 0;
    for (i = 0; i < MAX_CH_NUM; ++i) n += !!CH_com_buf[i].valid;
    return n;
}
uint8_t scan_name_cache_count(void) {
    uint8_t i, n = 0;
    for (i = 0; i < SCAN_NAME_CACHE_NUM; ++i) n += g_scan_name_cache[i].name[0] != 0;
    return n;
}
const char *scan_name_cache_name(uint8_t i) {
    return i < SCAN_NAME_CACHE_NUM ? (const char *)g_scan_name_cache[i].name : NULL;
}

/* Hardware endpoints are inert. They do not emulate the device or scheduler. */
void u8x8_InitDisplay(u8x8_t *u) { (void)u; }
void u8x8_SetPowerSave(u8x8_t *u, uint8_t a) { (void)u; (void)a; }
#undef u8x8_gpio_SetCS
uint8_t u8x8_gpio_SetCS(u8x8_t *u, uint8_t a) { (void)u; (void)a; return 1; }
void u8g2_Setup_st7305_yuying_168x384_f(u8g2_t *u, const u8g2_cb_t *a, u8x8_msg_cb b, u8x8_msg_cb c) { (void)u; (void)a; (void)b; (void)c; }
void u8g2_Setup_st7306_300x400_f(u8g2_t *u, const u8g2_cb_t *a, u8x8_msg_cb b, u8x8_msg_cb c) { (void)u; (void)a; (void)b; (void)c; }
void u8x8_gpio_call(u8x8_t *u, uint8_t m, uint8_t a) { (void)u; (void)m; (void)a; }
uint8_t u8x8_DrawTile(u8x8_t *u, uint8_t x, uint8_t y, uint8_t n, uint8_t *p) { (void)u; (void)x; (void)y; (void)n; (void)p; return 1; }
uint8_t u8g2_GetKerning(u8g2_t *u, u8g2_kerning_t *k, uint16_t a, uint16_t b) { (void)u; (void)k; (void)a; (void)b; return 0; }
uint8_t u8g2_GetKerningByTable(u8g2_t *u, const uint16_t *k, uint16_t a, uint16_t b) { (void)u; (void)k; (void)a; (void)b; return 0; }
void u8g2_SetDrawColor(u8g2_t *u, uint8_t c) { u->draw_color = c; }
void u8g2_ClearBuffer(u8g2_t *u) { (void)u; memset(pixels, 0, sizeof(pixels)); }
void u8g2_SendBuffer(u8g2_t *u) { (void)u; }
/* Retain out-of-bounds drawing requests so the validator can observe clipping. */
uint8_t u8g2_IsIntersection(u8g2_t *u, u8g2_uint_t a, u8g2_uint_t b, u8g2_uint_t c, u8g2_uint_t d) { (void)u; (void)a; (void)b; (void)c; (void)d; return 1; }

static void point(u8g2_t *u, int x, int y) {
    if (custom_glyph_active) {
        ++custom_points;
        if (x < custom_left) custom_left = x;
        if (y < custom_top) custom_top = y;
        if (x > custom_right) custom_right = x;
        if (y > custom_bottom) custom_bottom = y;
    }
    /* White foreground glyphs (draw_color=0) have bounds too. */
    if (text_active) {
        ++text_points;
        if (x < text_left) text_left = x;
        if (y < text_top) text_top = y;
        if (x > text_right) text_right = x;
        if (y > text_bottom) text_bottom = y;
    }
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) {
        if (!text_active) {
            if (boundary_total < 100) fprintf(draw_log, "PIXEL_OUTSIDE\t%s\t%s\t%d\t%d\n", case_name, draw_pass, x, y);
            ++boundary_total;
        }
        return;
    }
    if (u->draw_color == 2) pixels[y][x] ^= 1;
    else pixels[y][x] = !!u->draw_color;
}
void u8g2_DrawHVLine(u8g2_t *u, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t n, uint8_t dir) {
    unsigned i;
    int xx = (int16_t)x, yy = (int16_t)y;
    for (i = 0; i < n; ++i)
        point(u, xx + (dir == 0 ? (int)i : dir == 2 ? -(int)i : 0),
                 yy + (dir == 1 ? (int)i : dir == 3 ? -(int)i : 0));
}
void u8g2_DrawHLine(u8g2_t *u, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t n) { u8g2_DrawHVLine(u, x, y, n, 0); }
void u8g2_DrawVLine(u8g2_t *u, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t n) { u8g2_DrawHVLine(u, x, y, n, 1); }
void u8g2_DrawPixel(u8g2_t *u, u8g2_uint_t x, u8g2_uint_t y) { point(u, (int16_t)x, (int16_t)y); }

static void check_box(const char *kind, unsigned x, unsigned y, unsigned w, unsigned h) {
    int outside = x + w > SCREEN_WIDTH || y + h > SCREEN_HEIGHT;
    fprintf(draw_log, "%s\t%s\t%s\t%u\t%u\t%u\t%u\t%d\n", kind, case_name, draw_pass, x, y, w, h, outside);
    if (outside) ++boundary_total;
}
void u8g2_DrawFrame(u8g2_t *u, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, u8g2_uint_t h) { check_box("FRAME", x, y, w, h); real_u8g2_DrawFrame(u, x, y, w, h); }
void u8g2_DrawBox(u8g2_t *u, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, u8g2_uint_t h) { check_box("BOX", x, y, w, h); real_u8g2_DrawBox(u, x, y, w, h); }
void u8g2_DrawRFrame(u8g2_t *u, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, u8g2_uint_t h, u8g2_uint_t r) { check_box("RFRAME", x, y, w, h); real_u8g2_DrawRFrame(u, x, y, w, h, r); }
void u8g2_DrawRBox(u8g2_t *u, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, u8g2_uint_t h, u8g2_uint_t r) { check_box("RBOX", x, y, w, h); real_u8g2_DrawRBox(u, x, y, w, h, r); }
void u8g2_DrawXBM(u8g2_t *u, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, u8g2_uint_t h, const uint8_t *b) { check_box("XBM", x, y, w, h); real_u8g2_DrawXBM(u, x, y, w, h, b); }
void u8g2_DrawXBMP(u8g2_t *u, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, u8g2_uint_t h, const uint8_t *b) {
    check_box("XBMP", x, y, w, h);
    if (home_page_expect && !strcmp(draw_pass, "primary") &&
        ((w == 13u && h == 11u) || (w == 17u && h == 13u) ||
         (w == 21u && h == 17u) || (w == 29u && h == 21u))) {
        ++home_wifi_count;
        home_wifi_x = x;
        home_wifi_y = y;
        home_wifi_width = w;
        home_wifi_height = h;
    }
    if (home_page_expect && !strcmp(draw_pass, "primary") && x == 9u && y == 5u && w >= 60u && h >= 30u) {
        ++home_logo_count;
        home_logo_width = w;
        home_logo_height = h;
        if (w == 88u && h == 44u) home_new_layout = 1u;
    }
    /* Generated glyph canvases have these exact heights. Observe actual ink,
     * rather than treating blank advance columns as overlapping text. */
    custom_glyph_active = !(w == 17u && h == 13u) &&
        w <= 24u && (h == 13u || h == 16u || h == 18u || h == 20u);
    real_u8g2_DrawXBMP(u, x, y, w, h, b);
    custom_glyph_active = 0;
}

static const char *font_name(const uint8_t *f) {
    if (f == u8g2_font_wqy14_t_gb2312a) return "wqy14";
    if (f == u8g2_font_helvB10_tr) return "helvB10";
    if (f == u8g2_font_helvB12_tr) return "helvB12";
    if (f == u8g2_font24_lunar) return "lunar24";
    if (f == u8g2_font16_lunar) return "lunar16";
    if (f == u8g2_font_5x8_tr) return "5x8";
    return "other_project_font";
}
u8g2_uint_t u8g2_DrawUTF8(u8g2_t *u, u8g2_uint_t x, u8g2_uint_t y, const char *s) {
    unsigned i, missing = 0;
    u8x8_t utf = {0};
    uint16_t e;
    char miss[512] = "", tmp[16];
    u8g2_uint_t w = u8g2_GetUTF8Width(u, s), advance;
    if (home_page_expect && u->font == u8g2_font_helvB10_tr) {
        /* 主页只有切角通道号用 helvB10 画纯数字，据此收集本页通道号集合。 */
        unsigned k, digits = 1;
        for (k = 0; s[k]; ++k) if (s[k] < '0' || s[k] > '9') { digits = 0; break; }
        if (digits && k > 0 && k < 4 && badge_count < sizeof(badge_text) / sizeof(badge_text[0]))
            strcpy(badge_text[badge_count++], s);
    }
    for (i = 0; s[i]; ++i) {
        e = u8x8_utf8_next(&utf, (uint8_t)s[i]);
        if (e < 0xfffe && !u8g2_IsGlyph(u, e)) {
            ++missing;
            snprintf(tmp, sizeof(tmp), "U+%04X,", e);
            strncat(miss, tmp, sizeof(miss) - strlen(miss) - 1);
        }
    }
    text_left = text_top = 32767;
    text_right = text_bottom = -32768;
    text_points = 0;
    text_active = 1;
    advance = real_u8g2_DrawUTF8(u, x, y, s);
    text_active = 0;
    if (!text_points) text_left = text_top = text_right = text_bottom = -1;
    fprintf(draw_log, "TEXT\t%s\t%s\t%s\t%u\t%u\t%u\t%d\t%d\t%d\t%d\t%u\t%s\t%s\n",
            case_name, draw_pass, font_name(u->font), x, y, w,
            text_left, text_top, text_right, text_bottom, missing, miss, s);
    missing_total += missing;
    if (text_points && (text_left < 0 || text_top < 0 ||
        text_right >= SCREEN_WIDTH || text_bottom >= SCREEN_HEIGHT)) ++boundary_total;
    return advance;
}

/* Observe the firmware's real bitmap font path, including missing characters. */
void ui_preview_text(uint16_t x, uint16_t y, const char *text, uint8_t size, unsigned missing) {
    fprintf(draw_log, "CUSTOM_TEXT\t%s\t%s\t%u\t%u\t%u\t%u\t%s\n",
            case_name, draw_pass, x, y, size, missing, text);
    fprintf(draw_log, "CUSTOM_INK\t%s\t%s\t%u\t%u\t%u\t%d\t%d\t%d\t%d\t%s\n",
            case_name, draw_pass, x, y, size,
            custom_points ? custom_left : -1, custom_points ? custom_top : -1,
            custom_points ? custom_right : -1, custom_points ? custom_bottom : -1, text);
    custom_left = custom_top = 32767;
    custom_right = custom_bottom = -32768;
    custom_points = 0;
    missing_total += missing;
    if (home_page_expect && !strcmp(draw_pass, "primary")) {
        unsigned k, digits = 1;
        for (k = 0; text[k]; ++k) if (text[k] < '0' || text[k] > '9') { digits = 0; break; }
        if (digits && k && k < 4 && (size == 11u || size == 14u) &&
            (x < 40u || (x >= 203u && x < 225u)) && y >= 60u && y <= 158u && badge_count < 32u)
            strcpy(badge_text[badge_count++], text);
        if (size == 18u) home_new_layout = 1u;
        if (!strncmp(text, "分站号:", strlen("分站号:")) || !strcmp(text, "台") ||
            (!strncmp(text, "状态:", strlen("状态:")) && y == 20u)) home_relocated_layout = 1u;
        if (!strncmp(text, "状态", strlen("状态")) ||
            !strcmp(text, "开机") || !strcmp(text, "关机")) {
            ++home_state_count;
            home_state_baseline = y;
        }
        if (!strncmp(text, "已绑定:", strlen("已绑定:")) ||
            !strncmp(text, "已用通道:", strlen("已用通道:")) ||
            !strncmp(text, "报警:", strlen("报警:"))) {
            ++home_stats_count;
            home_stats_baseline = y;
            if (y >= 52u) ++home_header_bottom;
        }
    }
}

static void init_graphics(void) {
    memset(&u8g2, 0, sizeof(u8g2));
    u8g2.width = SCREEN_WIDTH;
    u8g2.height = SCREEN_HEIGHT;
    u8g2.draw_color = 1;
    u8g2_SetFontPosBaseline(&u8g2);
    u8g2_SetFont(&u8g2, UI_FONT_CN);
    u8g2_SetFontMode(&u8g2, 1);
}
static void save_pixels(const char *name) {
    unsigned x, y;
    char path[160];
    FILE *f;
    snprintf(path, sizeof(path), "%s.pgm", name);
    f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "Cannot save %s\n", path); exit(2); }
    fprintf(f, "P5\n%d %d\n255\n", SCREEN_WIDTH, SCREEN_HEIGHT);
    for (y = 0; y < SCREEN_HEIGHT; ++y)
        for (x = 0; x < SCREEN_WIDTH; ++x) fputc(pixels[y][x] ? 0 : 255, f);
    fclose(f);
}
static void checked_draw(void) {
    data_LIST data_before = Data_list1;
    device_t channels_before[MAX_CH_NUM];
    scan_binding bindings_before[MAX_BINDING_NUM];
    scan_name_entry_t scan_before[SCAN_NAME_CACHE_NUM];
    uint8_t binding_count_before = g_binding_count;
    uint8_t name_errors_before = g_name_err_count, voltage_errors_before = g_volt_err_count;
    uint32_t name_seconds_before = g_name_chk_sec;
    uint8_t message_before = g_ui_msg;
    uint8_t bitmap_mode_before = u8g2.bitmap_transparency;
    custom_left = custom_top = 32767;
    custom_right = custom_bottom = -32768;
    custom_points = 0;
    memcpy(channels_before, CH_com_buf, sizeof(channels_before));
    memcpy(bindings_before, g_binding_list, sizeof(bindings_before));
    memcpy(scan_before, g_scan_name_cache, sizeof(scan_before));
    UI_Control(&Data_list1);
    if (bitmap_mode_before != u8g2.bitmap_transparency) {
        ++bitmap_state_total;
        fprintf(draw_log, "BITMAP_MODE_CHANGED\t%s\t%s\t%u\t%u\n", case_name, draw_pass,
                bitmap_mode_before, u8g2.bitmap_transparency);
    }
    if (memcmp(&data_before, &Data_list1, sizeof(data_before)) ||
        memcmp(channels_before, CH_com_buf, sizeof(channels_before)) ||
        memcmp(bindings_before, g_binding_list, sizeof(bindings_before)) ||
        memcmp(scan_before, g_scan_name_cache, sizeof(scan_before)) ||
        binding_count_before != g_binding_count || name_errors_before != g_name_err_count ||
        voltage_errors_before != g_volt_err_count || name_seconds_before != g_name_chk_sec ||
        message_before != g_ui_msg) {
        ++mutation_total;
        fprintf(draw_log, "SOURCE_DATA_CHANGED\t%s\t%s\n", case_name, draw_pass);
    }
}

static void check_home_contract(unsigned page);

static void render(const char *name, unsigned rank, unsigned menu, unsigned re, unsigned selected, unsigned page) {
    static uint8_t primary[SCREEN_HEIGHT][SCREEN_WIDTH];
    uint8_t bitmap_input = u8g2.bitmap_transparency;
    unsigned missing_before = missing_total, boundary_before = boundary_total;
    unsigned mutation_before = mutation_total, redraw_before = redraw_total;
    unsigned bitmap_before = bitmap_state_total;
    unsigned region_before = region_violation_total + badge_violation_total;
    case_name = name;
    Data_list1.menu_rank = (uint8_t)rank;
    Data_list1.rank2_addr = (uint8_t)menu;
    Data_list1.UI_main.re_flag = (uint8_t)re;
    Data_list1.rank3_addr = (uint8_t)selected;
    Data_list1.UI_main.chu_num1 = (uint16_t)page;
    Data_list1.UI_main.chu_num2 = (uint16_t)page;
    draw_pass = "primary";
    badge_count = home_state_count = home_stats_count = 0;
    home_logo_count = home_logo_width = home_logo_height = home_new_layout = 0;
    home_header_bottom = home_state_baseline = home_stats_baseline = 0;
    home_relocated_layout = home_wifi_count = 0;
    home_wifi_x = home_wifi_y = 0;
    home_wifi_width = home_wifi_height = 0;
    checked_draw();
    memcpy(primary, pixels, sizeof(primary));
    save_pixels(name);
    if (home_page_expect) check_home_contract(home_page_expect);
    draw_pass = "continuous";
    checked_draw();
    if (memcmp(primary, pixels, sizeof(primary))) ++redraw_total;
    init_graphics();
    u8g2_SetBitmapMode(&u8g2, bitmap_input);
    draw_pass = "independent";
    checked_draw();
    if (memcmp(primary, pixels, sizeof(primary))) ++redraw_total;
    fprintf(case_log, "%s\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\n",
            name, rank, menu, re, selected, page,
            missing_total - missing_before, boundary_total - boundary_before,
            mutation_total - mutation_before, redraw_total - redraw_before,
            bitmap_state_total - bitmap_before,
            region_violation_total + badge_violation_total - region_before);
    ++cases_total;
}

/* Recognize the frozen legacy layout from its actual draw calls. The enlarged
 * layout puts each page's metadata above identical grids and uses one logo.
 * Channel numbers remain 1..10 or 11..20 on the corresponding page. */
static void check_home_contract(unsigned page) {
    unsigned i, j, ok = 1;
    unsigned first_ch = (page == 2u) ? 11u : 1u;
    unsigned status_ink = home_new_layout ? (page == 1u ? ink_in_region(102u, 34u, 372u, 46u) : 0u) : page == 1u ? ink_in_region(14u, 43u, 374u, 60u) :
        ink_in_region(HOME_STATUS_REGION_X0, HOME_STATUS_REGION_Y0,
                      HOME_STATUS_REGION_X1, HOME_STATUS_REGION_Y1);
    unsigned stats_ink = home_new_layout ? (page == 2u ? ink_in_region(102u, 34u, 372u, 46u) : 0u) : ink_in_region(HOME_STATS_REGION_X0, HOME_STATS_REGION_Y0,
                                       HOME_STATS_REGION_X1, HOME_STATS_REGION_Y1);

    for (i = 0; i < 10u; ++i) {
        char want[4];
        sprintf(want, "%u", first_ch + i);
        for (j = 0; j < badge_count; ++j) if (!strcmp(badge_text[j], want)) break;
        if (j == badge_count) ok = 0;
    }
    for (j = 0; j < badge_count; ++j) {
        unsigned value = (unsigned)atoi(badge_text[j]);
        if (page == 1u ? (value < 1u || value > 10u) : (value < 11u || value > 20u)) ok = 0;
    }
    if (badge_count != 10u) ok = 0;
    if (!ok) {
        ++badge_violation_total;
        fprintf(draw_log, "HOME_CHANNEL_SET\t%s\t%u\t%u\n", case_name, page, badge_count);
    }
    if (home_new_layout && (home_logo_count != 1u || home_logo_width != 88u || home_logo_height != 44u ||
        home_header_bottom || (home_relocated_layout ?
            (page == 2u && ((home_state_baseline != 20u && home_state_baseline != 23u && home_state_baseline != 44u) || home_stats_baseline != 44u)) :
            (page == 1u ? home_state_baseline != 44u : home_stats_baseline != 44u)))) {
        ++region_violation_total;
        fprintf(draw_log, "HOME_HEADER_CONTRACT\t%s\t%u\t%u\t%u\t%u\n", case_name, page,
                home_logo_count, home_logo_width, home_logo_height);
    }
    if (page == 1u) {
        if (status_ink < home_p1_status_min_ink) home_p1_status_min_ink = status_ink;
        if (stats_ink > home_p1_stats_max_ink) home_p1_stats_max_ink = stats_ink;
        if (home_state_count != (home_relocated_layout ? 0u : 1u) || home_stats_count) ++region_violation_total;
    } else {
        if (status_ink > home_p2_status_max_ink) home_p2_status_max_ink = status_ink;
        if (stats_ink < home_p2_stats_min_ink) home_p2_stats_min_ink = stats_ink;
        if (home_state_count != (home_relocated_layout ? 1u : 0u) ||
            home_stats_count != (home_relocated_layout ? 2u : 3u) || status_ink || !stats_ink)
            ++region_violation_total;
    }
    if (home_relocated_layout) {
        unsigned x, y, wing_failures = 0;
        unsigned wifi_ink = home_wifi_count ?
            ink_in_region(home_wifi_x, home_wifi_y, home_wifi_x + home_wifi_width - 1u,
                          home_wifi_y + home_wifi_height - 1u) : 0u;
        uint32_t wifi_hash = 2166136261u;
        for (y = 4u; y < 24u; ++y) {
            unsigned left = 78u + y - 2u;
            unsigned gap = 85u + y - 4u;
            for (x = left; x < left + 11u; ++x) {
                unsigned expected = x != gap && x != gap + 1u;
                if (pixels[y][x] != expected || pixels[y][383u - x] != expected) ++wing_failures;
            }
        }
        if (home_wifi_count)
            for (y = home_wifi_y; y < home_wifi_y + home_wifi_height; ++y)
                for (x = home_wifi_x; x < home_wifi_x + home_wifi_width; ++x)
                    wifi_hash = (wifi_hash ^ pixels[y][x]) * 16777619u;
        if (wing_failures || home_wifi_count != (page == 1u ? 1u : 0u) ||
            (page == 2u && wifi_ink)) ++region_violation_total;
        fprintf(draw_log, "HOME_TOP_LAYOUT\t%s\t%u\t%u\t%u\t%u\t%u\n", case_name, page,
                home_wifi_count, wifi_hash, wifi_ink, wing_failures);
    }
    fprintf(draw_log, "HOME_REGIONS\t%s\t%u\t%u\t%u\n", case_name, page, status_ink, stats_ink);
}

static void render_home(const char *name, unsigned page) {
    home_page_expect = page;
    render(name, 1, 0, 0, 0, page);
    home_page_expect = 0;
}

static void init_data(int maximum) {
    static const Sensor_Tpye types[10] = {TPYE_MG, TPYE_JG, TPYE_WY2, TPYE_LF, TPYE_QJ,
        TPYE_YL, TPYE_YW, TPYE_WZ, TPYE_DY, TPYE_MG};
    static const uint16_t values[10] = {523, 128, 235, 6, (uint16_t)(int16_t)-12, 186, 310, 42, 87, 631};
    unsigned i, j;
    init_graphics();
    ui_show_msg(UI_MSG_NONE);
    memset(&Data_list1, 0, sizeof(Data_list1));
    memset(CH_com_buf, 0, sizeof(CH_com_buf));
    memset(g_binding_list, 0, sizeof(g_binding_list));
    memset(g_scan_name_cache, 0, sizeof(g_scan_name_cache));
    Data_list1.UI_main.host_num = maximum ? 65535 : 41;
    Data_list1.UI_main.sub_num = maximum ? 65535 : 30;
    Data_list1.UI_main.send_host_num = maximum ? 65535 : 38;
    Data_list1.UI_main.Lora_rssi = maximum ? 255 : 11;
    Data_list1.UI_main.version = maximum ? 65535 : 10;
    Data_list1.UI_main.vbat = maximum ? 65535 : 365;
    Data_list1.UI_main.state = 1;
    Data_list1.Menu_rank1.set_host_num = 41;
    Data_list1.Menu_rank1.set_sub_num = 30;
    Data_list1.Menu_rank2.xuhao_num = 1;
    Data_list1.Menu_rank2.now_host_addr = 41;
    Data_list1.Menu_rank2.text_host_addr = 38;
    Data_list1.Menu_rank2.text_cnt = 5;
    Data_list1.Menu_rank2.bl_numl = 3;
    Data_list1.Menu_rank6.power = 10;
    Data_list1.Menu_rank6.time_light = 30;
    Data_list1.Menu_rank6.state = 1;
    for (i = 0; i < 3; ++i) {
        Data_list1.Menu_rank5.old_send_addr[i] = (uint16_t)(38 + i);
        Data_list1.Menu_rank5.new_send_addr[i] = (uint16_t)(38 + i);
    }
    g_binding_count = maximum ? MAX_BINDING_NUM : 3;
    g_name_err_count = maximum ? 16 : 1;
    g_volt_err_count = maximum ? 255 : 1;
    g_name_chk_sec = 0;
    for (i = 0; i < MAX_BINDING_NUM; ++i) {
        snprintf((char *)g_binding_list[i].name, MAX_NAME_LEN, "SW_%02u_WY_01-04", i + 1);
        for (j = 0; j < 6; ++j) g_binding_list[i].mac[j] = (uint8_t)(i + j);
    }
    for (i = 0; i < MAX_CH_NUM; ++i) {
        CH_com_buf[i].valid = 1;
        CH_com_buf[i].data_re_flag = 1;
        CH_com_buf[i].Type = types[i % 10];
        CH_com_buf[i].CH_data = maximum ? (CH_com_buf[i].Type == TPYE_QJ ? (uint16_t)(int16_t)-32768 : 65535) : values[i % 10];
        CH_com_buf[i].voltage = maximum ? 255 : 35;
        CH_com_buf[i].rssi = (uint8_t)(maximum ? -128 : -65);
        memcpy(CH_com_buf[i].name, g_binding_list[i].name, MAX_NAME_LEN);
    }
    for (i = 0; i < SCAN_NAME_CACHE_NUM; ++i)
        memcpy(g_scan_name_cache[i].name, g_binding_list[i].name, MAX_NAME_LEN);
}

static void longest_names(void) {
    unsigned i;
    for (i = 0; i < MAX_BINDING_NUM; ++i) {
        memset(g_binding_list[i].name, 'W', MAX_NAME_LEN - 1);
        g_binding_list[i].name[MAX_NAME_LEN - 1] = 0;
    }
    for (i = 0; i < MAX_CH_NUM; ++i)
        memcpy(CH_com_buf[i].name, g_binding_list[i].name, MAX_NAME_LEN);
    for (i = 0; i < SCAN_NAME_CACHE_NUM; ++i)
        memcpy(g_scan_name_cache[i].name, g_binding_list[i].name, MAX_NAME_LEN);
}

static void render_menu_cases(void) {
    static const unsigned focus_count[8] = {6, 2, 4, 3, 5, 8, 5, 1};
    static const uint16_t address_values[] = {0, 121, 122, 65535};
    static const uint16_t pages[] = {0, 1, 2, 65535};
    static const int8_t rssi_values[] = {-128, -1, 0, 127};
    static const uint8_t voltage_values[] = {0, 9, 10, 99, 100, 255};
    unsigned m, s, re, i, n, mode, extreme;
    char name[100];

    init_data(0);
    for (m = 0; m < 8; ++m) {
        for (s = 0; s < focus_count[m]; ++s) {
            snprintf(name, sizeof(name), "menu_all_%u_focus_%u", m, s);
            render(name, 3, m, 0, s, 1);
        }
    }
    /* Obsolete STM32 focus IDs are rendered defensively but are not selectable
     * on the two-action networking screen. This does not simulate MCU key logic. */
    for (s = 2; s <= 3; ++s) {
        snprintf(name, sizeof(name), "menu_networking_obsolete_focus_%u", s);
        render(name, 3, 1, 0, s, 1);
    }
    for (re = 1; re <= 6; ++re) {
        n = (re == 1 || re == 2 || re == 6) ? 2 : 1;
        for (s = 0; s < n; ++s) {
            snprintf(name, sizeof(name), "subpage_%u_button_%u", re, s);
            render(name, 3, re <= 2 ? 2 : re <= 4 ? 3 : 6, re, s, 1);
        }
    }
    /* A stale fullscreen flag must not steal another menu, especially menu 7. */
    for (re = 1; re <= 5; ++re) {
        if (re == 2) continue;
        snprintf(name, sizeof(name), "route_return_left_stale_%u", re);
        render(name, 2, 7, re, 0, 1);
        for (m = 0; m < 8; ++m) {
            if ((m == 2 && re == 1) || (m == 3 && (re == 3 || re == 4)) || (m == 6 && re == 5)) continue;
            snprintf(name, sizeof(name), "route_menu_%u_stale_%u", m, re);
            render(name, 3, m, re, 0, 1);
        }
    }
    for (mode = 0; mode <= 1; ++mode) {
        for (m = 0; m < 8; ++m) {
            u8g2_SetBitmapMode(&u8g2, (uint8_t)mode);
            snprintf(name, sizeof(name), "menu_%u_bitmap_%u_left", m, mode);
            render(name, 2, m, 0, 0, 1);
            u8g2_SetBitmapMode(&u8g2, (uint8_t)mode);
            snprintf(name, sizeof(name), "menu_%u_bitmap_%u_right", m, mode);
            render(name, 3, m, 0, focus_count[m] - 1, 1);
        }
    }

    init_data(0);
    g_binding_count = 0;
    memset(g_binding_list, 0, sizeof(g_binding_list));
    memset(g_scan_name_cache, 0, sizeof(g_scan_name_cache));
    render("menu_binding_empty", 3, 2, 0, 0, 1);
    for (re = 1; re <= 2; ++re) {
        for (s = 0; s < 2; ++s) {
            snprintf(name, sizeof(name), "subpage_%u_empty_button_%u", re, s);
            render(name, 3, 2, re, s, 1);
        }
    }
    for (n = 10; n <= 11; ++n) {
        init_data(0);
        g_binding_count = (uint8_t)n;
        for (i = 0; i < n; ++i) {
            snprintf((char *)g_binding_list[i].name, MAX_NAME_LEN, "SW_MG_1_%u", i + 1);
            for (m = 0; m < 6; ++m) g_binding_list[i].mac[m] = (uint8_t)(i + m);
        }
        for (i = 0; i < 2; ++i) {
            g_name_chk_sec = i * 4;
            snprintf(name, sizeof(name), "binding_count_%u_cycle_%u", n, i);
            render(name, 3, 2, 1, 0, 1);
        }
    }
    /* Unbind lists use the full normal 20-channel capacity without timer paging.
     * The 32-record case verifies defensive clipping while retaining the raw count. */
    {
        static const unsigned counts[] = {0, 1, 7, 10, 11, 20, 32};
        static const unsigned times[] = {0, 4, 8, 12, 65535};
        unsigned k, cycle;
        for (k = 0; k < sizeof(counts) / sizeof(counts[0]); ++k) {
            init_data(0);
            n = counts[k];
            g_binding_count = (uint8_t)n;
            for (cycle = 0; cycle < sizeof(times) / sizeof(times[0]); ++cycle) {
                g_name_chk_sec = times[cycle];
                for (s = 0; s < 2; ++s) {
                    snprintf(name, sizeof(name), "subpage_unbind_count_%u_time_%u_button_%u", n, times[cycle], s);
                    render(name, 3, 2, 1, s, 1);
                }
            }
        }
        for (mode = 0; mode < 3; ++mode) {
            init_data(0);
            longest_names();
            g_binding_count = MAX_CH_NUM;
            for (i = 0; i < MAX_CH_NUM; ++i)
                for (m = 0; m < 6; ++m)
                    g_binding_list[i].mac[m] = mode == 0 ? 0xDDu : mode == 1 ? 0x00u :
                                               (uint8_t)(m % 3u == 0u ? 0xABu : m % 3u == 1u ? 0xCDu : 0xEFu);
            for (cycle = 0; cycle < 2; ++cycle) {
                g_name_chk_sec = cycle * 12u;
                for (s = 0; s < 2; ++s) {
                    snprintf(name, sizeof(name), "subpage_unbind_mac_%u_cycle_%u_button_%u", mode, cycle, s);
                    render(name, 3, 2, 1, s, 1);
                }
            }
        }
    }
    init_data(0);
    render("binding_left_stale_1", 2, 2, 1, 0, 1);
    render("binding_left_stale_2", 2, 2, 2, 0, 1);
    render("binding_save_parent_stale", 2, 2, 2, 0, 2);
    for (n = 1; n <= SCAN_NAME_CACHE_NUM; ++n) {
        init_data(0);
        for (i = n; i < SCAN_NAME_CACHE_NUM; ++i)
            memset(g_scan_name_cache[i].name, 0, MAX_NAME_LEN);
        for (s = 0; s < 2; ++s) {
            snprintf(name, sizeof(name), "subpage_scan_count_%u_button_%u", n, s);
            render(name, 3, 2, 2, s, 1);
        }
    }
    /* Bind-page drawing fixtures deliberately keep the scan cache independent.
     * 32 records exercise defensive RAM-table capacity, not valid channel allocation. */
    {
        static const unsigned counts[] = {0, 1, 6, 7, 10, 11, 20, 32};
        unsigned k, cycle, timer_samples;
        for (k = 0; k < sizeof(counts) / sizeof(counts[0]); ++k) {
            n = counts[k];
            timer_samples = n ? (n + 9u) / 10u : 1u;
            for (mode = 0; mode < 2; ++mode) {
                init_data(0);
                g_binding_count = (uint8_t)n;
                memset(g_scan_name_cache, 0, sizeof(g_scan_name_cache));
                if (mode)
                    for (i = 0; i < SCAN_NAME_CACHE_NUM; ++i)
                        snprintf((char *)g_scan_name_cache[i].name, MAX_NAME_LEN, "CACHE_ONLY_%u", i + 1);
                for (cycle = 0; cycle <= timer_samples; ++cycle) {
                    g_name_chk_sec = cycle * 4u;
                    for (s = 0; s < 2; ++s) {
                        snprintf(name, sizeof(name), "subpage_scan_bound_%u_cache_%u_cycle_%u_button_%u", n, mode, cycle, s);
                        render(name, 3, 2, 2, s, 1);
                    }
                }
            }
        }
    }
    init_data(1);
    longest_names();
    render("menu_binding_full", 3, 2, 0, 0, 1);
    for (i = 0; i < (MAX_BINDING_NUM + 9) / 10; ++i) {
        g_name_chk_sec = i * 4;
        for (s = 0; s < 2; ++s) {
            snprintf(name, sizeof(name), "subpage_binding_full_cycle_%u_button_%u", i, s);
            render(name, 3, 2, 1, s, 1);
        }
    }
    for (i = 0; i <= (MAX_BINDING_NUM + 9) / 10; ++i) {
        g_name_chk_sec = i * 4u;
        for (s = 0; s < 2; ++s) {
            snprintf(name, sizeof(name), "subpage_scan_longest_cycle_%u_button_%u", i, s);
            render(name, 3, 2, 2, s, 1);
        }
    }
    init_data(0);
    for (i = 0; i < sizeof(address_values) / sizeof(address_values[0]); ++i) {
        Data_list1.UI_main.version = address_values[i];
        snprintf(name, sizeof(name), "return_version_%u", address_values[i]);
        render(name, 2, 7, 0, 0, 1);
    }
    init_data(1);
    longest_names();
    for (i = 0; i < sizeof(pages) / sizeof(pages[0]); ++i) {
        snprintf(name, sizeof(name), "menu_names_page_%u", pages[i]);
        render(name, 3, 6, 5, 0, pages[i]);
    }

    init_data(0);
    for (i = 0; i < MAX_CH_NUM; ++i) {
        CH_com_buf[i].rssi = (uint8_t)rssi_values[i % 4];
        CH_com_buf[i].voltage = voltage_values[i % 6];
    }
    for (i = 0; i < sizeof(pages) / sizeof(pages[0]); ++i) {
        snprintf(name, sizeof(name), "menu_signal_page_%u", pages[i]);
        render(name, 3, 3, 3, 0, pages[i]);
        snprintf(name, sizeof(name), "menu_voltage_page_%u", pages[i]);
        render(name, 3, 3, 4, 0, pages[i]);
    }
    memset(CH_com_buf, 0, sizeof(CH_com_buf));
    for (re = 3; re <= 5; ++re) {
        snprintf(name, sizeof(name), "subpage_%u_channels_empty", re);
        render(name, 3, re == 5 ? 6 : 3, re, 0, 1);
    }
    init_data(0);
    CH_com_buf[0].valid = 0;
    CH_com_buf[1].name[0] = 0;
    for (re = 3; re <= 5; ++re) {
        snprintf(name, sizeof(name), "subpage_%u_channels_sparse", re);
        render(name, 3, re == 5 ? 6 : 3, re, 0, 1);
    }

    for (i = 0; i < sizeof(address_values) / sizeof(address_values[0]); ++i) {
        init_data(0);
        for (m = 0; m < 3; ++m) {
            Data_list1.Menu_rank5.old_send_addr[m] = address_values[i];
            Data_list1.Menu_rank5.new_send_addr[m] = address_values[i];
        }
        for (s = 0; s < focus_count[4]; ++s) {
            snprintf(name, sizeof(name), "menu_upload_value_%u_focus_%u", address_values[i], s);
            render(name, 3, 4, 0, s, 1);
        }
        Data_list1.Menu_rank2.net_flag = 1;
        Data_list1.Menu_rank2.now_host_addr = address_values[i];
        Data_list1.Menu_rank2.text_host_addr = address_values[i];
        snprintf(name, sizeof(name), "networking_address_%u", address_values[i]);
        render(name, 3, 1, 0, 0, 1);
    }
    for (extreme = 0; extreme <= 1; ++extreme) {
        init_data(0);
        Data_list1.Menu_rank1.set_host_num = extreme ? 65535 : 0;
        Data_list1.Menu_rank1.set_sub_num = extreme ? 65535 : 0;
        for (s = 0; s < focus_count[0]; ++s) {
            snprintf(name, sizeof(name), "menu_addr_%s_focus_%u", extreme ? "max" : "zero", s);
            render(name, 3, 0, 0, s, 1);
        }
        Data_list1.Menu_rank2.net_flag = 1;
        Data_list1.Menu_rank2.xuhao_num = extreme ? 65535 : 0;
        Data_list1.Menu_rank2.now_host_addr = extreme ? 65535 : 0;
        Data_list1.Menu_rank2.text_host_addr = extreme ? 65535 : 0;
        Data_list1.Menu_rank2.text_cnt = extreme ? 65535 : 0;
        Data_list1.Menu_rank2.bl_numl = extreme ? 65535 : 0;
        snprintf(name, sizeof(name), "networking_values_%s", extreme ? "max" : "zero");
        render(name, 3, 1, 0, 0, 1);
        Data_list1.Menu_rank6.power = (char)(extreme ? 127 : -128);
        Data_list1.Menu_rank6.time_light = extreme ? 65535 : 0;
        Data_list1.Menu_rank6.state = (uint8_t)extreme;
        for (s = 0; s < focus_count[5]; ++s) {
            snprintf(name, sizeof(name), "menu_other_%s_focus_%u", extreme ? "max" : "min", s);
            render(name, 3, 5, 0, s, 1);
        }
        g_binding_count = extreme ? MAX_BINDING_NUM : 0;
        g_name_err_count = extreme ? NAME_CHK_ERR_NUM : 0;
        g_volt_err_count = extreme ? 255 : 0;
        for (s = 0; s < focus_count[6]; ++s) {
            snprintf(name, sizeof(name), "menu_summary_%s_focus_%u", extreme ? "max" : "zero", s);
            render(name, 3, 6, 0, s, 1);
        }
    }

    /* Simulate the existing controller frame sequence, not physical K2 events. */
    init_data(0);
    for (re = 3u; re <= 5u; ++re) {
        unsigned menu = re == 5u ? 6u : 3u;
        snprintf(name, sizeof(name), "pagination_%u_enter", re);
        render(name, 3, menu, re, 0, 1);
        snprintf(name, sizeof(name), "pagination_%u_next", re);
        render(name, 3, menu, re, 0, 2);
        snprintf(name, sizeof(name), "pagination_%u_exit", re);
        render(name, 3, menu, 0, 0, 1);
        snprintf(name, sizeof(name), "pagination_%u_reenter", re);
        render(name, 3, menu, re, 0, 1);
    }

    /* Distinct column endpoints reveal skipped, duplicated or shifted channels. */
    init_data(0);
    {
        static const unsigned endpoints[] = {0, 9, 10, 19};
        static const uint8_t values[] = {11, 42, 73, 104};
        for (i = 0; i < 4; ++i) {
            unsigned ch = endpoints[i];
            CH_com_buf[ch].valid = CH_com_buf[ch].data_re_flag = 1;
            CH_com_buf[ch].rssi = (uint8_t)(int8_t)-(int)values[i];
            CH_com_buf[ch].voltage = values[i];
            snprintf((char *)CH_com_buf[ch].name, MAX_NAME_LEN, "SW_CH%02u", ch + 1);
        }
    }
    render("menu_signal_endpoints", 3, 3, 3, 0, 1);
    render("menu_voltage_endpoints", 3, 3, 4, 0, 1);
    render("menu_names_endpoints", 3, 6, 5, 0, 1);

    init_data(0);
    for (i = 0; i < MAX_CH_NUM; ++i) CH_com_buf[i].data_re_flag = 0;
    render("menu_20_signal_waiting", 3, 3, 3, 0, 1);
    render("menu_20_voltage_waiting", 3, 3, 4, 0, 1);
    render("menu_20_names_waiting", 3, 6, 5, 0, 1);
    for (i = 0; i < MAX_CH_NUM; ++i) {
        CH_com_buf[i].data_re_flag = 1;
        CH_com_buf[i].rssi = CH_com_buf[i].voltage = 0;
    }
    render("menu_20_signal_received_zero", 3, 3, 3, 0, 1);
    render("menu_20_voltage_received_zero", 3, 3, 4, 0, 1);
    CH_com_buf[19].data_re_flag = 0;
    render("menu_20_signal_partial_received", 3, 3, 3, 0, 1);
    render("menu_20_voltage_partial_received", 3, 3, 4, 0, 1);
    strcpy((char *)CH_com_buf[9].name, "g_jpqy");
    strcpy((char *)CH_com_buf[19].name, "g_jpqy");
    render("menu_20_names_descenders", 3, 6, 5, 0, 1);
    render("menu_20_names_descenders_page2", 3, 6, 5, 0, 2);

    /* Carry font, color and bitmap context across alternating actual pages. */
    init_data(0);
    for (i = 0; i < 3; ++i) {
        snprintf(name, sizeof(name), "transition_binding_%u", i);
        render(name, 3, 2, 1, i % 2, 1);
        snprintf(name, sizeof(name), "transition_voltage_%u", i);
        render(name, 3, 3, 4, 0, 1);
        snprintf(name, sizeof(name), "transition_menu_%u", i);
        render(name, 2, 5, 0, 0, 1);
        snprintf(name, sizeof(name), "home_after_menus_%u", i);
        render(name, 1, 0, 0, 0, 1);
        ui_show_msg(UI_MSG_PARAM_SAVE_OK);
        snprintf(name, sizeof(name), "message_on_menu_%u", i);
        render(name, 3, 6, 6, 0, 1);
        ui_show_msg(UI_MSG_NONE);
    }
}

static unsigned check_white_text_bounds(void) {
    unsigned before, failures = 0;
    init_graphics();
    u8g2_SetFont(&u8g2, u8g2_font_5x8_tr);
    u8g2_SetDrawColor(&u8g2, 0);
    case_name = "validator_white_text";
    before = boundary_total;
    u8g2_DrawUTF8(&u8g2, SCREEN_WIDTH, 20, "W");
    if (boundary_total == before) ++failures;
    before = boundary_total;
    u8g2_DrawUTF8(&u8g2, (u8g2_uint_t)-40, 20, "W");
    if (boundary_total == before) ++failures;
    boundary_total = missing_total = 0;
    init_graphics();
    return failures;
}

int main(int argc, char **argv) {
    unsigned m, s, re;
    unsigned endpoint_failures;
    const char *scope = argc > 1 ? argv[1] : "home";
    char name[100];
    FILE *summary;
    draw_log = fopen("draws.tsv", "wb");
    case_log = fopen("cases.tsv", "wb");
    if (!draw_log || !case_log) return 2;
    if (strcmp(scope, "home") && strcmp(scope, "menus") && strcmp(scope, "unified")) return 2;
    endpoint_failures = check_white_text_bounds();
    fprintf(case_log, "case\trank\tmenu\tsubpage\tselected\tpage\tmissing_glyphs\tboundary_events\tdata_mutations\tredraw_mismatches\tbitmap_state_changes\tregion_violations\n");
    init_data(0);
    /* Screenshot example: ten active channels; the second page is tested separately. */
    memset(CH_com_buf + 10, 0, sizeof(CH_com_buf[0]) * 10);
    render_home("home_example", 1);
    init_data(0);
    render_home("home_page2", 2);
    for (m = 0; m < 12; ++m) {
        static const uint8_t signals[] = {0u, 1u, 2u, 3u, 4u, 7u, 8u, 9u, 10u, 11u, 12u, 255u};
        init_data(0);
        Data_list1.UI_main.Lora_rssi = signals[m];
        snprintf(name, sizeof(name), "home_wifi_%u", signals[m]);
        render_home(name, 1);
    }
    /* Exercise all HEX codes and fallback with the compact bitmap too. */
    for (m = 8; m <= 12; ++m) {
        init_data(0);
        Data_list1.UI_main.Lora_rssi = (uint8_t)m;
        Data_list1.UI_main.vbat = 65535u;
        snprintf(name, sizeof(name), "home_wifi_compact_%u", m);
        render_home(name, 1);
    }
    for (m = 0; m < 4; ++m) {
        static const uint16_t voltages[] = {999u, 1000u, 9999u, 10000u};
        init_data(0);
        Data_list1.UI_main.vbat = voltages[m];
        snprintf(name, sizeof(name), "home_voltage_%u", Data_list1.UI_main.vbat);
        render_home(name, 1);
    }
    for (m = 0; m < 2; ++m) {
        for (s = 0; s < 3; ++s) {
            init_data(0);
            Data_list1.UI_main.state = (uint8_t)m;
            g_binding_count = (uint8_t)(s == 2u ? MAX_BINDING_NUM : s ? 20u : 0u);
            snprintf(name, sizeof(name), "home_page2_state_%s_bound_%u", m ? "on" : "off", g_binding_count);
            render_home(name, 2);
        }
    }
    init_data(0);
    u8g2_SetBitmapMode(&u8g2, 1);
    render_home("home_bitmap_mode_1", 1);
    u8g2_SetBitmapMode(&u8g2, 0);
    render_home("home_bitmap_mode_0", 1);
    for (m = 0; m < 3; ++m) {
        static const uint16_t addresses[] = {0, 121, 122};
        Data_list1.UI_main.send_host_num = addresses[m];
        snprintf(name, sizeof(name), "home_send_%u", addresses[m]);
        render_home(name, 1);
    }
    Data_list1.UI_main.send_host_num = UINT16_MAX;
    render_home("home_send_numeric_max", 1);
    Data_list1.UI_main.host_num = 0;
    render_home("home_host_zero", 1);
    Data_list1.UI_main.host_num = UINT16_MAX;
    render_home("home_host_max", 1);
    Data_list1.UI_main.host_num = 41;
    Data_list1.UI_main.sub_num = UINT16_MAX;
    render_home("home_sub_max", 1);
    Data_list1.UI_main.state = 0;
    render_home("home_state_off", 1);
    Data_list1.UI_main.host_num = 118;
    Data_list1.UI_main.sub_num = 64;
    Data_list1.UI_main.send_host_num = 122;
    render_home("home_state_off_station", 1);
    init_data(1);
    render_home("home_uint16_max", 1);
    render_home("home_page2_uint16_max", 2);
    CH_com_buf[4].CH_data = (uint16_t)(int16_t)32767;
    render_home("home_tilt_positive_max", 1);
    memset(CH_com_buf, 0, sizeof(CH_com_buf));
    g_binding_count = g_name_err_count = g_volt_err_count = 0;
    render_home("home_empty", 1);
    render_home("home_page2_empty", 2);
    init_data(0);
    CH_com_buf[0].Type = TPYE_NONE;
    CH_com_buf[1].Type = TPYE_END;
    CH_com_buf[2].valid = 0;
    render_home("home_invalid_types", 1);
    init_data(0);
    for (m = 0; m < 8; ++m) {
        snprintf(name, sizeof(name), "menu_%u", m);
        render(name, 2, m, 0, 0, 1);
        for (s = 0; s < 2; ++s) {
            snprintf(name, sizeof(name), "menu_%u_focus_%u", m, s);
            render(name, 3, m, 0, s, 1);
        }
    }
    Data_list1.Menu_rank2.net_flag = 1;
    render("networking_active", 3, 1, 0, 0, 1);
    for (re = 1; re <= 6; ++re) {
        snprintf(name, sizeof(name), "subpage_%u", re);
        render(name, 3, re <= 2 ? 2 : re <= 4 ? 3 : 6, re, 0, 1);
    }
    for (s = 1; s <= 9; ++s) {
        ui_show_msg((uint8_t)s);
        snprintf(name, sizeof(name), "message_%u", s);
        render(name, 1, 0, 0, 0, 1);
    }
    ui_show_msg(UI_MSG_PARAM_SAVE_OK);
    (void)ui_msg_tick_sec();
    render("save_message_tick1", 1, 0, 0, 0, 1);
    (void)ui_msg_tick_sec();
    render("save_message_tick2_home", 1, 0, 0, 0, 1);
    ui_show_msg(UI_MSG_POWER_OFF);
    for (s = 0; s < 10; ++s) (void)ui_msg_tick_sec();
    render("power_off_message_tick10", 1, 0, 0, 0, 1);
    ui_show_msg(UI_MSG_NONE);
    render("restart_confirmation", 4, 0, 0, 0, 1);
    if (!strcmp(scope, "menus") || !strcmp(scope, "unified")) render_menu_cases();
    if (!strcmp(scope, "home") || !strcmp(scope, "unified")) {
        init_data(0);
        for (m = 0; m < MAX_CH_NUM; ++m) CH_com_buf[m].data_re_flag = 0;
        render_home("home_bound_waiting", 1);
        render_home("home_page2_bound_waiting", 2);
        if (!strcmp(scope, "unified")) {
            render("menu_signal_waiting", 3, 3, 3, 0, 1);
            render("menu_signal_waiting_page2", 3, 3, 3, 0, 2);
            render("menu_voltage_waiting", 3, 3, 4, 0, 1);
            render("menu_voltage_waiting_page2", 3, 3, 4, 0, 2);
            render("menu_names_waiting", 3, 6, 5, 0, 1);
            render("menu_names_waiting_page2", 3, 6, 5, 0, 2);
        }
        for (m = 0; m < MAX_CH_NUM; ++m) {
            CH_com_buf[m].data_re_flag = 1;
            CH_com_buf[m].CH_data = CH_com_buf[m].rssi = CH_com_buf[m].voltage = 0;
        }
        render_home("home_received_zero", 1);
        render_home("home_page2_received_zero", 2);
        if (!strcmp(scope, "unified")) {
            render("menu_signal_received_zero", 3, 3, 3, 0, 1);
            render("menu_signal_received_zero_page2", 3, 3, 3, 0, 2);
            render("menu_voltage_received_zero", 3, 3, 4, 0, 1);
            render("menu_voltage_received_zero_page2", 3, 3, 4, 0, 2);
        }
        CH_com_buf[0].data_re_flag = 0;
        CH_com_buf[19].data_re_flag = 0;
        render_home("home_partial_received", 1);
        if (!strcmp(scope, "unified")) {
            render("menu_signal_partial_received", 3, 3, 3, 0, 1);
            render("menu_signal_partial_received_page2", 3, 3, 3, 0, 2);
            render("menu_voltage_partial_received", 3, 3, 4, 0, 1);
            render("menu_voltage_partial_received_page2", 3, 3, 4, 0, 2);
            strcpy((char *)CH_com_buf[9].name, "g_jpqy");
            strcpy((char *)CH_com_buf[19].name, "g_jpqy");
            render("menu_names_descenders", 3, 6, 5, 0, 1);
            render("menu_names_descenders_page2", 3, 6, 5, 0, 2);
        }
    }
    fclose(draw_log);
    fclose(case_log);
    summary = fopen("summary.json", "wb");
    if (!summary) return 2;
    fprintf(summary, "{\n  \"preview_scope\": \"%s\",\n  \"cases\": %u,\n  \"missing_glyph_occurrences\": %u,\n  \"boundary_events\": %u,\n  \"data_mutations\": %u,\n  \"redraw_mismatches\": %u,\n  \"bitmap_state_changes\": %u,\n  \"home_contract_violations\": %u,\n  \"home_page1_status_region_min_ink\": %u,\n  \"home_page1_stats_region_max_ink\": %u,\n  \"home_page2_status_region_max_ink\": %u,\n  \"home_page2_stats_region_min_ink\": %u,\n  \"white_text_boundary_probe_failures\": %u,\n  \"text_bounds_include_draw_color_zero\": true\n}\n",
            scope, cases_total, missing_total, boundary_total, mutation_total, redraw_total,
            bitmap_state_total, region_violation_total + badge_violation_total,
            home_p1_status_min_ink, home_p1_stats_max_ink, home_p2_status_max_ink,
            home_p2_stats_min_ink, endpoint_failures);
    fclose(summary);
    printf("cases=%u missing=%u boundary_events=%u data_mutations=%u redraw_mismatches=%u\n",
           cases_total, missing_total, boundary_total, mutation_total, redraw_total);
    return (missing_total || boundary_total || mutation_total || redraw_total ||
            bitmap_state_total || region_violation_total || badge_violation_total ||
            endpoint_failures) ? 1 : 0;
}
